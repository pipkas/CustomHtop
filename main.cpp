#include <arpa/inet.h>
#include <csignal>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <optional>
#include <pwd.h>
#include <regex>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

std::string read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string escape_json(const std::string& value) {
    std::ostringstream out;
    for (unsigned char ch : value) {
        switch (ch) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (ch < 0x20) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(ch) << std::dec << std::setfill(' ');
                } else {
                    out << ch;
                }
        }
    }
    return out.str();
}

bool starts_with(const std::string& value, const std::string& prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

std::string lower_copy(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::optional<long long> json_int_value(const std::string& body, const std::string& key) {
    const std::regex pattern("\"" + key + "\"\\s*:\\s*(-?[0-9]+)");
    std::smatch match;
    if (!std::regex_search(body, match, pattern)) {
        return std::nullopt;
    }
    try {
        return std::stoll(match[1].str());
    } catch (...) {
        return std::nullopt;
    }
}

std::string format_time_plus(unsigned long long jiffies, long ticks_per_second) {
    const double seconds_exact = static_cast<double>(jiffies) / static_cast<double>(ticks_per_second);
    const auto centiseconds = static_cast<unsigned long long>(seconds_exact * 100.0);
    const auto hours = centiseconds / 360000;
    const auto minutes = (centiseconds / 6000) % 60;
    const auto seconds = (centiseconds / 100) % 60;
    const auto centi = centiseconds % 100;

    std::ostringstream out;
    out << hours << ':'
        << std::setw(2) << std::setfill('0') << minutes << ':'
        << std::setw(2) << seconds << '.'
        << std::setw(2) << centi << std::setfill(' ');
    return out.str();
}

std::string state_name(char state) {
    switch (state) {
        case 'R': return "running";
        case 'S': return "sleeping";
        case 'D': return "uninterruptible sleep";
        case 'Z': return "zombie";
        case 'T': return "stopped";
        case 't': return "tracing stop";
        case 'I': return "idle";
        case 'X':
        case 'x': return "dead";
        case 'K': return "wakekill";
        case 'W': return "waking";
        case 'P': return "parked";
        default: return "unknown";
    }
}

std::string signal_name(int signum) {
    static const std::map<int, std::string> names = {
#ifdef SIGHUP
        {SIGHUP, "SIGHUP"},
#endif
#ifdef SIGINT
        {SIGINT, "SIGINT"},
#endif
#ifdef SIGQUIT
        {SIGQUIT, "SIGQUIT"},
#endif
#ifdef SIGILL
        {SIGILL, "SIGILL"},
#endif
#ifdef SIGTRAP
        {SIGTRAP, "SIGTRAP"},
#endif
#ifdef SIGABRT
        {SIGABRT, "SIGABRT"},
#endif
#ifdef SIGBUS
        {SIGBUS, "SIGBUS"},
#endif
#ifdef SIGFPE
        {SIGFPE, "SIGFPE"},
#endif
#ifdef SIGKILL
        {SIGKILL, "SIGKILL"},
#endif
#ifdef SIGUSR1
        {SIGUSR1, "SIGUSR1"},
#endif
#ifdef SIGSEGV
        {SIGSEGV, "SIGSEGV"},
#endif
#ifdef SIGUSR2
        {SIGUSR2, "SIGUSR2"},
#endif
#ifdef SIGPIPE
        {SIGPIPE, "SIGPIPE"},
#endif
#ifdef SIGALRM
        {SIGALRM, "SIGALRM"},
#endif
#ifdef SIGTERM
        {SIGTERM, "SIGTERM"},
#endif
#ifdef SIGSTKFLT
        {SIGSTKFLT, "SIGSTKFLT"},
#endif
#ifdef SIGCHLD
        {SIGCHLD, "SIGCHLD"},
#endif
#ifdef SIGCONT
        {SIGCONT, "SIGCONT"},
#endif
#ifdef SIGSTOP
        {SIGSTOP, "SIGSTOP"},
#endif
#ifdef SIGTSTP
        {SIGTSTP, "SIGTSTP"},
#endif
#ifdef SIGTTIN
        {SIGTTIN, "SIGTTIN"},
#endif
#ifdef SIGTTOU
        {SIGTTOU, "SIGTTOU"},
#endif
#ifdef SIGURG
        {SIGURG, "SIGURG"},
#endif
#ifdef SIGXCPU
        {SIGXCPU, "SIGXCPU"},
#endif
#ifdef SIGXFSZ
        {SIGXFSZ, "SIGXFSZ"},
#endif
#ifdef SIGVTALRM
        {SIGVTALRM, "SIGVTALRM"},
#endif
#ifdef SIGPROF
        {SIGPROF, "SIGPROF"},
#endif
#ifdef SIGWINCH
        {SIGWINCH, "SIGWINCH"},
#endif
#ifdef SIGIO
        {SIGIO, "SIGIO"},
#endif
#ifdef SIGPWR
        {SIGPWR, "SIGPWR"},
#endif
#ifdef SIGSYS
        {SIGSYS, "SIGSYS"},
#endif
    };

    const auto found = names.find(signum);
    if (found != names.end()) {
        return found->second;
    }

#ifdef SIGRTMIN
    const int rt_min = SIGRTMIN;
    const int rt_max = SIGRTMAX;
    if (signum >= rt_min && signum <= rt_max) {
        std::ostringstream name;
        name << "SIGRTMIN";
        if (signum > rt_min) {
            name << '+' << (signum - rt_min);
        }
        return name.str();
    }
#endif

    std::ostringstream fallback;
    fallback << "SIG" << signum;
    return fallback.str();
}

struct MemInfo {
    unsigned long long mem_total = 0;
    unsigned long long mem_free = 0;
    unsigned long long mem_available = 0;
    unsigned long long buffers = 0;
    unsigned long long cache = 0;
    unsigned long long swap_total = 0;
    unsigned long long swap_free = 0;

    unsigned long long mem_used() const {
        if (mem_total <= mem_free + buffers + cache) {
            return 0;
        }
        return mem_total - mem_free - buffers - cache;
    }

    unsigned long long swap_used() const {
        if (swap_total <= swap_free) {
            return 0;
        }
        return swap_total - swap_free;
    }
};

struct CpuTimes {
    int id = -1;
    unsigned long long user = 0;
    unsigned long long nice = 0;
    unsigned long long system = 0;
    unsigned long long idle = 0;
    unsigned long long iowait = 0;
    unsigned long long irq = 0;
    unsigned long long softirq = 0;
    unsigned long long steal = 0;

    unsigned long long total() const {
        return user + nice + system + idle + iowait + irq + softirq + steal;
    }

    unsigned long long idle_all() const {
        return idle + iowait;
    }
};

struct CpuCoreView {
    int id = 0;
    double total = 0.0;
    double low = 0.0;
    double high = 0.0;
    double kernel = 0.0;
    double background = 0.0;
};

struct ProcessInfo {
    int pid = 0;
    int ppid = 0;
    int pgrp = 0;
    int session = 0;
    long tty_nr = 0;
    long priority = 0;
    long nice = 0;
    long threads = 0;
    char state = '?';
    std::string user;
    std::string comm;
    std::string command;
    std::string time_plus;
    unsigned long long virt_bytes = 0;
    unsigned long long rss_bytes = 0;
    unsigned long long shared_bytes = 0;
    unsigned long long total_jiffies = 0;
    unsigned long long start_time = 0;
    double cpu_percent = 0.0;
    double mem_percent = 0.0;
};

struct ProcSample {
    unsigned long long total_jiffies = 0;
    unsigned long long start_time = 0;
};

class ProcessSampler {
public:
    std::string snapshot_json() {
        std::lock_guard<std::mutex> lock(mutex_);

        const auto now = std::chrono::steady_clock::now();
        const MemInfo mem = read_meminfo();
        std::vector<CpuCoreView> cpu_views = read_cpu_views();
        std::vector<ProcessInfo> processes = read_processes(mem, now);

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

private:
    MemInfo read_meminfo() {
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

    std::vector<CpuTimes> read_cpu_times() {
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

    std::vector<CpuCoreView> read_cpu_views() {
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

    std::string user_name_for_uid(uid_t uid) {
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

    uid_t read_uid(int pid) {
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

    std::string read_command(int pid, const std::string& comm) {
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

    void read_statm(ProcessInfo& proc) {
        std::istringstream input(read_file("/proc/" + std::to_string(proc.pid) + "/statm"));
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

    std::optional<ProcessInfo> read_process(int pid) {
        const std::string stat = read_file("/proc/" + std::to_string(pid) + "/stat");
        const std::size_t left = stat.find('(');
        const std::size_t right = stat.rfind(')');
        if (stat.empty() || left == std::string::npos || right == std::string::npos || right <= left + 1) {
            return std::nullopt;
        }

        ProcessInfo proc;
        proc.pid = pid;
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

        proc.user = user_name_for_uid(read_uid(pid));
        proc.command = read_command(pid, proc.comm);
        proc.time_plus = format_time_plus(proc.total_jiffies, ticks_per_second_);
        read_statm(proc);
        return proc;
    }

    std::vector<ProcessInfo> read_processes(const MemInfo& mem, std::chrono::steady_clock::time_point now) {
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
            current_samples[pid] = ProcSample{proc.total_jiffies, proc.start_time};

            const auto previous = previous_processes_.find(pid);
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
            processes.push_back(std::move(proc));
        }
        closedir(dir);

        previous_processes_ = std::move(current_samples);
        last_process_sample_time_ = now;
        return processes;
    }

    std::mutex mutex_;
    long ticks_per_second_ = sysconf(_SC_CLK_TCK) > 0 ? sysconf(_SC_CLK_TCK) : 100;
    unsigned long long page_size_ = sysconf(_SC_PAGESIZE) > 0 ? static_cast<unsigned long long>(sysconf(_SC_PAGESIZE)) : 4096ULL;
    std::unordered_map<int, CpuTimes> previous_cpus_;
    std::unordered_map<int, ProcSample> previous_processes_;
    std::optional<std::chrono::steady_clock::time_point> last_process_sample_time_;
    std::unordered_map<uid_t, std::string> users_;
};

struct HttpRequest {
    std::string method;
    std::string target;
    std::string path;
    std::string body;
    std::unordered_map<std::string, std::string> headers;
};

class HttpServer {
public:
    explicit HttpServer(int port) : port_(port) {}

    int run() {
        signal(SIGPIPE, SIG_IGN);

        const int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            std::cerr << "socket: " << std::strerror(errno) << '\n';
            return 1;
        }

        int yes = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(static_cast<uint16_t>(port_));

        if (bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
            std::cerr << "bind: " << std::strerror(errno) << '\n';
            close(server_fd);
            return 1;
        }

        if (listen(server_fd, 64) < 0) {
            std::cerr << "listen: " << std::strerror(errno) << '\n';
            close(server_fd);
            return 1;
        }

        std::cout << "CustomHtop WebUI: http://127.0.0.1:" << port_ << "\n";
        std::cout << "Press Ctrl+C to stop.\n";

        while (true) {
            sockaddr_in client{};
            socklen_t client_len = sizeof(client);
            const int client_fd = accept(server_fd, reinterpret_cast<sockaddr*>(&client), &client_len);
            if (client_fd < 0) {
                if (errno == EINTR) {
                    continue;
                }
                std::cerr << "accept: " << std::strerror(errno) << '\n';
                continue;
            }
            std::thread(&HttpServer::handle_client, this, client_fd).detach();
        }
    }

private:
    bool read_request(int fd, HttpRequest& request) {
        std::string data;
        char buffer[8192];
        const std::size_t max_request_size = 2 * 1024 * 1024;

        while (data.find("\r\n\r\n") == std::string::npos) {
            const ssize_t received = recv(fd, buffer, sizeof(buffer), 0);
            if (received <= 0) {
                return false;
            }
            data.append(buffer, static_cast<std::size_t>(received));
            if (data.size() > max_request_size) {
                return false;
            }
        }

        const std::size_t header_end = data.find("\r\n\r\n");
        const std::string header_text = data.substr(0, header_end);
        request.body = data.substr(header_end + 4);

        std::istringstream headers(header_text);
        std::string request_line;
        std::getline(headers, request_line);
        if (!request_line.empty() && request_line.back() == '\r') {
            request_line.pop_back();
        }

        std::istringstream first_line(request_line);
        first_line >> request.method >> request.target;
        if (request.method.empty() || request.target.empty()) {
            return false;
        }
        request.path = request.target;
        const std::size_t query = request.path.find('?');
        if (query != std::string::npos) {
            request.path = request.path.substr(0, query);
        }

        std::string line;
        while (std::getline(headers, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            const std::size_t colon = line.find(':');
            if (colon == std::string::npos) {
                continue;
            }
            request.headers[lower_copy(trim(line.substr(0, colon)))] = trim(line.substr(colon + 1));
        }

        std::size_t content_length = 0;
        const auto found_length = request.headers.find("content-length");
        if (found_length != request.headers.end()) {
            try {
                content_length = static_cast<std::size_t>(std::stoull(found_length->second));
            } catch (...) {
                return false;
            }
        }

        while (request.body.size() < content_length) {
            const ssize_t received = recv(fd, buffer, sizeof(buffer), 0);
            if (received <= 0) {
                return false;
            }
            request.body.append(buffer, static_cast<std::size_t>(received));
            if (request.body.size() > max_request_size) {
                return false;
            }
        }
        if (request.body.size() > content_length) {
            request.body.resize(content_length);
        }
        return true;
    }

    void send_response(int fd, int status, const std::string& type, const std::string& body) {
        const std::string reason = status_reason(status);
        std::ostringstream response;
        response << "HTTP/1.1 " << status << ' ' << reason << "\r\n"
                 << "Content-Type: " << type << "\r\n"
                 << "Content-Length: " << body.size() << "\r\n"
                 << "Cache-Control: no-store\r\n"
                 << "Connection: close\r\n\r\n";
        const std::string header = response.str();
        send_all(fd, header);
        send_all(fd, body);
    }

    void send_all(int fd, const std::string& data) {
        const char* ptr = data.data();
        std::size_t left = data.size();
        while (left > 0) {
            const ssize_t sent = send(fd, ptr, left, 0);
            if (sent <= 0) {
                return;
            }
            ptr += sent;
            left -= static_cast<std::size_t>(sent);
        }
    }

    std::string status_reason(int status) {
        switch (status) {
            case 200: return "OK";
            case 400: return "Bad Request";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            case 500: return "Internal Server Error";
            default: return "OK";
        }
    }

    std::string mime_type(const std::string& path) {
        if (path.size() >= 5 && path.substr(path.size() - 5) == ".html") {
            return "text/html; charset=utf-8";
        }
        if (path.size() >= 4 && path.substr(path.size() - 4) == ".css") {
            return "text/css; charset=utf-8";
        }
        if (path.size() >= 3 && path.substr(path.size() - 3) == ".js") {
            return "application/javascript; charset=utf-8";
        }
        if (path.size() >= 3 && path.substr(path.size() - 3) == ".ts") {
            return "text/plain; charset=utf-8";
        }
        return "application/octet-stream";
    }

    std::string static_path_for(const std::string& request_path) {
        if (request_path == "/") {
            return "web/index.html";
        }
        if (request_path.find("..") != std::string::npos) {
            return {};
        }
        return "web" + request_path;
    }

    void handle_client(int fd) {
        HttpRequest request;
        if (!read_request(fd, request)) {
            send_response(fd, 400, "application/json; charset=utf-8", "{\"ok\":false,\"error\":\"Bad request\"}");
            close(fd);
            return;
        }

        if (request.path == "/api/snapshot") {
            if (request.method != "GET") {
                send_response(fd, 405, "application/json; charset=utf-8", "{\"ok\":false,\"error\":\"Method not allowed\"}");
            } else {
                send_response(fd, 200, "application/json; charset=utf-8", sampler_.snapshot_json());
            }
            close(fd);
            return;
        }

        if (request.path == "/api/signals") {
            if (request.method != "GET") {
                send_response(fd, 405, "application/json; charset=utf-8", "{\"ok\":false,\"error\":\"Method not allowed\"}");
            } else {
                send_response(fd, 200, "application/json; charset=utf-8", signals_json());
            }
            close(fd);
            return;
        }

        if (request.path == "/api/signal") {
            if (request.method != "POST") {
                send_response(fd, 405, "application/json; charset=utf-8", "{\"ok\":false,\"error\":\"Method not allowed\"}");
            } else {
                send_response(fd, 200, "application/json; charset=utf-8", signal_process(request.body));
            }
            close(fd);
            return;
        }

        if (request.method != "GET") {
            send_response(fd, 405, "application/json; charset=utf-8", "{\"ok\":false,\"error\":\"Method not allowed\"}");
            close(fd);
            return;
        }

        const std::string file_path = static_path_for(request.path);
        if (file_path.empty()) {
            send_response(fd, 404, "text/plain; charset=utf-8", "Not found");
            close(fd);
            return;
        }

        const std::string content = read_file(file_path);
        if (content.empty()) {
            send_response(fd, 404, "text/plain; charset=utf-8", "Not found");
        } else {
            send_response(fd, 200, mime_type(file_path), content);
        }
        close(fd);
    }

    std::string signals_json() {
        std::ostringstream out;
        out << "{\"signals\":[";
        bool first = true;
        std::unordered_set<int> emitted;
        for (int signum = 1; signum < NSIG; ++signum) {
            if (emitted.count(signum) > 0) {
                continue;
            }
            const char* description = strsignal(signum);
            if (!description) {
                continue;
            }

            if (!first) {
                out << ',';
            }
            first = false;
            emitted.insert(signum);
            out << "{\"number\":" << signum
                << ",\"name\":\"" << escape_json(signal_name(signum)) << '"'
                << ",\"description\":\"" << escape_json(description) << "\"}";
        }
        out << "]}";
        return out.str();
    }

    std::string signal_process(const std::string& body) {
        const auto pid_value = json_int_value(body, "pid");
        const auto signal_value = json_int_value(body, "signal");
        if (!pid_value.has_value() || !signal_value.has_value() || *pid_value <= 0) {
            return "{\"ok\":false,\"error\":\"Invalid pid or signal\"}";
        }

        const int pid = static_cast<int>(*pid_value);
        const int signum = static_cast<int>(*signal_value);
        if (signum <= 0 || signum >= NSIG) {
            return "{\"ok\":false,\"error\":\"Signal is outside Linux signal range\"}";
        }

        if (kill(pid, signum) == 0) {
            std::ostringstream out;
            out << "{\"ok\":true,\"pid\":" << pid
                << ",\"signal\":" << signum
                << ",\"signalName\":\"" << escape_json(signal_name(signum)) << "\"}";
            return out.str();
        }

        const int err = errno;
        std::ostringstream out;
        out << "{\"ok\":false,\"pid\":" << pid
            << ",\"signal\":" << signum
            << ",\"signalName\":\"" << escape_json(signal_name(signum)) << '"'
            << ",\"errno\":" << err
            << ",\"error\":\"" << escape_json(std::strerror(err)) << "\"}";
        return out.str();
    }

    int port_;
    ProcessSampler sampler_;
};

} // namespace

int main(int argc, char** argv) {
    int port = 8080;
    if (const char* env_port = std::getenv("PORT")) {
        try {
            port = std::stoi(env_port);
        } catch (...) {
            std::cerr << "Invalid PORT, using 8080\n";
        }
    }
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {
            std::cerr << "Usage: " << argv[0] << " [port]\n";
            return 1;
        }
    }

    if (port <= 0 || port > 65535) {
        std::cerr << "Port must be in range 1..65535\n";
        return 1;
    }

    HttpServer server(port);
    return server.run();
}
