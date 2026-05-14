#include "ProcessSampler.h"

#include "FileUtil.h"
#include "JsonUtil.h"
#include "StateUtil.h"
#include "StringUtil.h"
#include "TimeUtil.h"

#include <algorithm>
#include <dirent.h>
#include <iomanip>
#include <map>
#include <pwd.h>
#include <sstream>
#include <unistd.h>

namespace custom_htop {

namespace {

int state_priority(char state) {
    switch (state) {
        case 'R': return 90;
        case 'D': return 80;
        case 'T':
        case 't': return 70;
        case 'Z': return 60;
        case 'S': return 50;
        case 'I': return 40;
        default: return 0;
    }
}

std::optional<char> state_from_stat(const std::string& stat) {
    const std::size_t left = stat.find('(');
    const std::size_t right = stat.rfind(')');
    if (stat.empty() || left == std::string::npos || right == std::string::npos || right <= left + 1) {
        return std::nullopt;
    }

    std::istringstream input(stat.substr(right + 2));
    char state = '?';
    input >> state;
    return input ? std::optional<char>(state) : std::nullopt;
}

}

ProcessSampler::ProcessSampler()
    : ticks_per_second_(sysconf(_SC_CLK_TCK) > 0 ? sysconf(_SC_CLK_TCK) : 100),
      page_size_(sysconf(_SC_PAGESIZE) > 0 ? static_cast<unsigned long long>(sysconf(_SC_PAGESIZE)) : 4096ULL) {}

std::string ProcessSampler::snapshot_json(bool include_threads) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto now = std::chrono::steady_clock::now();
    const MemInfo mem = read_meminfo();
    std::vector<CpuCoreView> cpu_views = read_cpu_views();
    std::vector<ProcessInfo> processes = read_processes(mem, now, include_threads);

    std::map<char, int> state_counts;
    for (const auto& proc : processes) {
        ++state_counts[proc.state];
    }

    std::ostringstream out;
    out << std::fixed << std::setprecision(2);
    out << '{';
    out << "\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() << ',';
    out << "\"memory\":{";
    out << "\"total\":" << mem.mem_total << ',';
    out << "\"used\":" << mem.mem_used() << ',';
    out << "\"free\":" << mem.mem_free << ',';
    out << "\"available\":" << mem.mem_available << ',';
    out << "\"buffers\":" << mem.buffers << ',';
    out << "\"cache\":" << mem.cache << "},";
    out << "\"swap\":{";
    out << "\"total\":" << mem.swap_total << ',';
    out << "\"used\":" << mem.swap_used() << ',';
    out << "\"free\":" << mem.swap_free << "},";
    out << "\"states\":{";
    bool first_state = true;
    for (const auto& item : state_counts) {
        if (!first_state) {
            out << ',';
        }
        first_state = false;
        out << '"' << escape_json(std::string(1, item.first)) << "\":" << item.second;
    }
    out << "},";
    out << "\"cpus\":[";
    for (std::size_t i = 0; i < cpu_views.size(); ++i) {
        if (i > 0) {
            out << ',';
        }
        const auto& cpu = cpu_views[i];
        out << "{\"id\":" << cpu.id
            << ",\"total\":" << cpu.total
            << ",\"low\":" << cpu.low
            << ",\"high\":" << cpu.high
            << ",\"kernel\":" << cpu.kernel
            << ",\"background\":" << cpu.background << '}';
    }
    out << "],";
    out << "\"processes\":[";
    for (std::size_t i = 0; i < processes.size(); ++i) {
        if (i > 0) {
            out << ',';
        }
        const auto& proc = processes[i];
        out << "{\"pid\":" << proc.pid
            << ",\"tid\":" << proc.tid
            << ",\"processPid\":" << proc.process_pid
            << ",\"signalPid\":" << proc.signal_pid
            << ",\"isThread\":" << (proc.is_thread ? "true" : "false")
            << ",\"ppid\":" << proc.ppid
            << ",\"pgrp\":" << proc.pgrp
            << ",\"session\":" << proc.session
            << ",\"ttyNr\":" << proc.tty_nr
            << ",\"user\":\"" << escape_json(proc.user) << '"'
            << ",\"priority\":" << proc.priority
            << ",\"nice\":" << proc.nice
            << ",\"threads\":" << proc.threads
            << ",\"state\":\"" << escape_json(std::string(1, proc.state)) << '"'
            << ",\"stateName\":\"" << escape_json(state_name(proc.state)) << '"'
            << ",\"cpu\":" << proc.cpu_percent
            << ",\"mem\":" << proc.mem_percent
            << ",\"virt\":" << proc.virt_bytes
            << ",\"res\":" << proc.rss_bytes
            << ",\"shr\":" << proc.shared_bytes
            << ",\"time\":\"" << escape_json(proc.time_plus) << '"'
            << ",\"comm\":\"" << escape_json(proc.comm) << '"'
            << ",\"command\":\"" << escape_json(proc.command) << "\"}";
    }
    out << "]}";
    return out.str();
}

MemInfo ProcessSampler::read_meminfo() {
    MemInfo mem;
    std::istringstream input(read_file("/proc/meminfo"));
    std::string key;
    unsigned long long value = 0;
    std::string unit;
    unsigned long long cached = 0;
    unsigned long long sreclaimable = 0;
    unsigned long long shmem = 0;

    while (input >> key >> value >> unit) {
        if (key == "MemTotal:") {
            mem.mem_total = value * 1024ULL;
        } else if (key == "MemFree:") {
            mem.mem_free = value * 1024ULL;
        } else if (key == "MemAvailable:") {
            mem.mem_available = value * 1024ULL;
        } else if (key == "Buffers:") {
            mem.buffers = value * 1024ULL;
        } else if (key == "Cached:") {
            cached = value * 1024ULL;
        } else if (key == "SReclaimable:") {
            sreclaimable = value * 1024ULL;
        } else if (key == "Shmem:") {
            shmem = value * 1024ULL;
        } else if (key == "SwapTotal:") {
            mem.swap_total = value * 1024ULL;
        } else if (key == "SwapFree:") {
            mem.swap_free = value * 1024ULL;
        }
    }

    mem.cache = cached + sreclaimable;
    if (mem.cache >= shmem) {
        mem.cache -= shmem;
    }
    return mem;
}

std::vector<CpuTimes> ProcessSampler::read_cpu_times() {
    std::vector<CpuTimes> cpus;
    std::istringstream input(read_file("/proc/stat"));
    std::string label;
    while (input >> label) {
        if (!starts_with(label, "cpu") || label == "cpu") {
            std::string rest;
            std::getline(input, rest);
            continue;
        }

        CpuTimes cpu;
        try {
            cpu.id = std::stoi(label.substr(3));
        } catch (...) {
            std::string rest;
            std::getline(input, rest);
            continue;
        }

        input >> cpu.user >> cpu.nice >> cpu.system >> cpu.idle >> cpu.iowait
              >> cpu.irq >> cpu.softirq >> cpu.steal;
        std::string rest;
        std::getline(input, rest);
        cpus.push_back(cpu);
    }
    return cpus;
}

std::vector<CpuCoreView> ProcessSampler::read_cpu_views() {
    const auto current = read_cpu_times();
    std::vector<CpuCoreView> views;
    views.reserve(current.size());

    for (const auto& cpu : current) {
        CpuTimes prev = cpu;
        const auto found = previous_cpus_.find(cpu.id);
        if (found != previous_cpus_.end()) {
            prev = found->second;
        }

        const unsigned long long total_delta = cpu.total() >= prev.total() ? cpu.total() - prev.total() : 0;
        const auto delta = [total_delta](unsigned long long now, unsigned long long old) {
            if (total_delta == 0 || now < old) {
                return 0.0;
            }
            return static_cast<double>(now - old) * 100.0 / static_cast<double>(total_delta);
        };

        const unsigned long long idle_delta = cpu.idle_all() >= prev.idle_all()
            ? cpu.idle_all() - prev.idle_all()
            : 0;
        CpuCoreView view;
        view.id = cpu.id;
        view.total = total_delta == 0
            ? 0.0
            : (static_cast<double>(total_delta - idle_delta) * 100.0 / static_cast<double>(total_delta));
        view.low = delta(cpu.nice, prev.nice);
        view.high = delta(cpu.user, prev.user);
        view.kernel = delta(cpu.system + cpu.irq + cpu.softirq, prev.system + prev.irq + prev.softirq);
        view.background = delta(cpu.iowait + cpu.steal, prev.iowait + prev.steal);
        views.push_back(view);
    }

    previous_cpus_.clear();
    for (const auto& cpu : current) {
        previous_cpus_[cpu.id] = cpu;
    }
    return views;
}

std::string ProcessSampler::user_name_for_uid(uid_t uid) {
    const auto cached = users_.find(uid);
    if (cached != users_.end()) {
        return cached->second;
    }

    const passwd* pwd = getpwuid(uid);
    std::string name;
    if (pwd && pwd->pw_name) {
        name = pwd->pw_name;
    } else {
        name = std::to_string(uid);
    }
    users_[uid] = name;
    return name;
}

uid_t ProcessSampler::read_uid(int pid) {
    std::istringstream input(read_file("/proc/" + std::to_string(pid) + "/status"));
    std::string line;
    while (std::getline(input, line)) {
        if (!starts_with(line, "Uid:")) {
            continue;
        }
        std::istringstream uid_line(line.substr(4));
        uid_t uid = 0;
        uid_line >> uid;
        return uid;
    }
    return 0;
}

std::string ProcessSampler::read_command(int pid, const std::string& comm) {
    std::string command = read_file("/proc/" + std::to_string(pid) + "/cmdline");
    for (char& ch : command) {
        if (ch == '\0') {
            ch = ' ';
        }
    }
    command = trim(command);
    if (command.empty()) {
        command = '[' + comm + ']';
    }
    return command;
}

void ProcessSampler::read_statm(ProcessInfo& proc, int process_pid) {
    std::istringstream input(read_file("/proc/" + std::to_string(process_pid) + "/statm"));
    unsigned long long size_pages = 0;
    unsigned long long resident_pages = 0;
    unsigned long long shared_pages = 0;
    input >> size_pages >> resident_pages >> shared_pages;
    if (size_pages > 0) {
        proc.virt_bytes = size_pages * page_size_;
    }
    if (resident_pages > 0) {
        proc.rss_bytes = resident_pages * page_size_;
    }
    proc.shared_bytes = shared_pages * page_size_;
}

std::optional<ProcessInfo> ProcessSampler::read_process(int pid) {
    return read_task(pid, pid, false);
}

std::optional<ProcessInfo> ProcessSampler::read_task(int process_pid, int task_pid, bool is_thread) {
    const std::string base = is_thread
        ? "/proc/" + std::to_string(process_pid) + "/task/" + std::to_string(task_pid)
        : "/proc/" + std::to_string(process_pid);
    const std::string stat = read_file(base + "/stat");
    const std::size_t left = stat.find('(');
    const std::size_t right = stat.rfind(')');
    if (stat.empty() || left == std::string::npos || right == std::string::npos || right <= left + 1) {
        return std::nullopt;
    }

    ProcessInfo proc;
    proc.pid = task_pid;
    proc.tid = task_pid;
    proc.process_pid = process_pid;
    proc.signal_pid = process_pid;
    proc.is_thread = is_thread;
    proc.comm = stat.substr(left + 1, right - left - 1);

    std::istringstream input(stat.substr(right + 2));
    input >> proc.state;
    std::vector<std::string> fields;
    std::string value;
    while (input >> value) {
        fields.push_back(value);
    }
    if (fields.size() < 21) {
        return std::nullopt;
    }

    try {
        proc.ppid = std::stoi(fields[0]);
        proc.pgrp = std::stoi(fields[1]);
        proc.session = std::stoi(fields[2]);
        proc.tty_nr = std::stol(fields[3]);
        proc.total_jiffies = std::stoull(fields[10]) + std::stoull(fields[11]);
        proc.priority = std::stol(fields[14]);
        proc.nice = std::stol(fields[15]);
        proc.threads = std::stol(fields[16]);
        proc.start_time = std::stoull(fields[18]);
        proc.virt_bytes = std::stoull(fields[19]);
        const long long rss_pages = std::stoll(fields[20]);
        proc.rss_bytes = rss_pages > 0 ? static_cast<unsigned long long>(rss_pages) * page_size_ : 0;
    } catch (...) {
        return std::nullopt;
    }

    if (is_thread) {
        proc.ppid = process_pid;
        proc.threads = 1;
    }
    proc.user = user_name_for_uid(read_uid(process_pid));
    proc.command = is_thread ? "{" + proc.comm + "}" : read_command(process_pid, proc.comm);
    proc.time_plus = format_time_plus(proc.total_jiffies, ticks_per_second_);
    read_statm(proc, process_pid);
    return proc;
}

std::vector<int> ProcessSampler::read_task_ids(int pid) {
    std::vector<int> task_ids;
    DIR* dir = opendir(("/proc/" + std::to_string(pid) + "/task").c_str());
    if (!dir) {
        return task_ids;
    }

    while (dirent* entry = readdir(dir)) {
        if (entry->d_type != DT_DIR && entry->d_type != DT_UNKNOWN) {
            continue;
        }
        const std::string name(entry->d_name);
        if (name.empty() || name.find_first_not_of("0123456789") != std::string::npos) {
            continue;
        }
        try {
            task_ids.push_back(std::stoi(name));
        } catch (...) {
        }
    }
    closedir(dir);
    std::sort(task_ids.begin(), task_ids.end());
    return task_ids;
}

char ProcessSampler::aggregate_process_state(int process_pid, char leader_state, const std::vector<int>& task_ids) {
    char best_state = leader_state;
    int best_priority = state_priority(leader_state);

    for (int tid : task_ids) {
        if (tid == process_pid) {
            continue;
        }

        const std::string stat = read_file(
            "/proc/" + std::to_string(process_pid) + "/task/" + std::to_string(tid) + "/stat");
        const auto state = state_from_stat(stat);
        if (!state.has_value()) {
            continue;
        }

        const int priority = state_priority(*state);
        if (priority > best_priority) {
            best_state = *state;
            best_priority = priority;
        }
    }

    return best_state;
}

void ProcessSampler::apply_sample(
    ProcessInfo& proc,
    const MemInfo& mem,
    double elapsed_seconds,
    std::unordered_map<int, ProcSample>& current_samples) {
    current_samples[proc.pid] = ProcSample{proc.total_jiffies, proc.start_time};

    const auto previous = previous_processes_.find(proc.pid);
    if (previous != previous_processes_.end()
        && previous->second.start_time == proc.start_time
        && elapsed_seconds > 0.0
        && proc.total_jiffies >= previous->second.total_jiffies) {
        const double delta_ticks = static_cast<double>(proc.total_jiffies - previous->second.total_jiffies);
        proc.cpu_percent = (delta_ticks / static_cast<double>(ticks_per_second_)) * 100.0 / elapsed_seconds;
    }

    if (mem.mem_total > 0) {
        proc.mem_percent = static_cast<double>(proc.rss_bytes) * 100.0 / static_cast<double>(mem.mem_total);
    }
}

std::vector<ProcessInfo> ProcessSampler::read_processes(
    const MemInfo& mem,
    std::chrono::steady_clock::time_point now,
    bool include_threads) {
    std::vector<ProcessInfo> processes;
    std::unordered_map<int, ProcSample> current_samples;

    double elapsed_seconds = 0.0;
    if (last_process_sample_time_.has_value()) {
        elapsed_seconds = std::chrono::duration<double>(now - *last_process_sample_time_).count();
    }

    DIR* dir = opendir("/proc");
    if (!dir) {
        return processes;
    }

    while (dirent* entry = readdir(dir)) {
        if (entry->d_type != DT_DIR && entry->d_type != DT_UNKNOWN) {
            continue;
        }
        const std::string name(entry->d_name);
        if (name.empty() || name.find_first_not_of("0123456789") != std::string::npos) {
            continue;
        }

        int pid = 0;
        try {
            pid = std::stoi(name);
        } catch (...) {
            continue;
        }

        auto proc_opt = read_process(pid);
        if (!proc_opt.has_value()) {
            continue;
        }

        ProcessInfo proc = *proc_opt;
        const std::vector<int> task_ids = read_task_ids(pid);
        proc.state = aggregate_process_state(pid, proc.state, task_ids);
        apply_sample(proc, mem, elapsed_seconds, current_samples);
        const std::string process_command = proc.command;
        processes.push_back(std::move(proc));

        if (!include_threads) {
            continue;
        }

        for (int tid : task_ids) {
            if (tid == pid) {
                continue;
            }
            auto thread_opt = read_task(pid, tid, true);
            if (!thread_opt.has_value()) {
                continue;
            }
            ProcessInfo thread = *thread_opt;
            thread.command = "{" + thread.comm + "} " + process_command;
            apply_sample(thread, mem, elapsed_seconds, current_samples);
            processes.push_back(std::move(thread));
        }
    }
    closedir(dir);

    previous_processes_ = std::move(current_samples);
    last_process_sample_time_ = now;
    return processes;
}

}
