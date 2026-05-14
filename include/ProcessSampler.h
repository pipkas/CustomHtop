#pragma once

#include "CpuCoreView.h"
#include "CpuTimes.h"
#include "MemInfo.h"
#include "ProcSample.h"
#include "ProcessInfo.h"

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <sys/types.h>
#include <unordered_map>
#include <vector>

namespace custom_htop {

/// Собирает данные о CPU, памяти, процессах и потоках из /proc и отдает готовый JSON-снимок.
class ProcessSampler {
public:
    /// Инициализирует системные константы: размер страницы памяти и частоту тиков ядра.
    ProcessSampler();

    /// Возвращает JSON со сводкой по системе; при include_threads добавляет строки потоков.
    std::string snapshot_json(bool include_threads = false);

private:
    /// Читает /proc/meminfo и нормализует значения памяти в байты.
    MemInfo read_meminfo();

    /// Читает сырые накопительные счетчики CPU из /proc/stat.
    std::vector<CpuTimes> read_cpu_times();

    /// Сравнивает текущие и предыдущие CPU-счетчики и строит процентные значения для UI.
    std::vector<CpuCoreView> read_cpu_views();

    /// Возвращает имя пользователя по UID с локальным кешированием результата.
    std::string user_name_for_uid(uid_t uid);

    /// Извлекает реальный UID владельца процесса из /proc/[pid]/status.
    uid_t read_uid(int pid);

    /// Читает командную строку процесса и подставляет comm, если cmdline пуст.
    std::string read_command(int pid, const std::string& comm);

    /// Дополняет ProcessInfo значениями виртуальной, резидентной и разделяемой памяти.
    void read_statm(ProcessInfo& proc, int process_pid);

    /// Читает процесс-лидер как обычную запись ProcessInfo.
    std::optional<ProcessInfo> read_process(int pid);

    /// Читает процесс или поток из /proc/[pid]/stat и связанных файлов.
    std::optional<ProcessInfo> read_task(int process_pid, int task_pid, bool is_thread);

    /// Возвращает отсортированный список TID из /proc/[pid]/task.
    std::vector<int> read_task_ids(int pid);

    /// Выбирает наиболее важное состояние среди процесса-лидера и его потоков.
    char aggregate_process_state(int process_pid, char leader_state, const std::vector<int>& task_ids);

    /// Заполняет CPU% и MEM%, сохраняя текущий снимок для следующего расчета.
    void apply_sample(
        ProcessInfo& proc,
        const MemInfo& mem,
        double elapsed_seconds,
        std::unordered_map<int, ProcSample>& current_samples);

    /// Обходит /proc, собирает процессы и при необходимости разворачивает потоки.
    std::vector<ProcessInfo> read_processes(
        const MemInfo& mem,
        std::chrono::steady_clock::time_point now,
        bool include_threads);

    std::mutex mutex_; ///< Защищает кеши между параллельными HTTP-запросами.
    long ticks_per_second_; ///< Количество тиков ядра в секунду.
    unsigned long long page_size_; ///< Размер страницы памяти в байтах.
    std::unordered_map<int, CpuTimes> previous_cpus_; ///< Предыдущие CPU-счетчики по id ядра.
    std::unordered_map<int, ProcSample> previous_processes_; ///< Предыдущие снимки процессов по PID/TID.
    std::optional<std::chrono::steady_clock::time_point> last_process_sample_time_; ///< Время предыдущего снимка процессов.
    std::unordered_map<uid_t, std::string> users_; ///< Кеш UID -> имя пользователя.
};

}
