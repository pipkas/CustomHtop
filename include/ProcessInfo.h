#pragma once

#include <string>

namespace custom_htop {

/// Снимок одного процесса или потока, собранный из /proc для JSON-ответа веб-интерфейса.
struct ProcessInfo {
    int pid = 0;                         ///< PID процесса или TID потока в режиме отображения потоков.
    int tid = 0;                         ///< Идентификатор конкретной задачи Linux.
    int process_pid = 0;                 ///< PID процесса-владельца, к которому относится запись.
    int signal_pid = 0;                  ///< PID, которому нужно отправлять сигналы из UI.
    int ppid = 0;                        ///< Родительский PID.
    int pgrp = 0;                        ///< Идентификатор группы процессов.
    int session = 0;                     ///< Идентификатор сессии процесса.
    long tty_nr = 0;                     ///< Номер управляющего терминала из /proc/[pid]/stat.
    long priority = 0;                   ///< Планировочный приоритет Linux.
    long nice = 0;                       ///< Значение nice.
    long threads = 0;                    ///< Количество потоков у процесса.
    bool is_thread = false;              ///< true, если запись описывает поток, а не процесс-лидер.
    char state = '?';                    ///< Однобуквенное состояние процесса Linux.
    std::string user;                    ///< Имя владельца процесса или числовой UID.
    std::string comm;                    ///< Короткое имя из поля comm.
    std::string command;                 ///< Полная командная строка или fallback-имя ядрового процесса.
    std::string time_plus;               ///< Суммарное CPU-время в формате TIME+.
    unsigned long long virt_bytes = 0;   ///< Виртуальная память процесса в байтах.
    unsigned long long rss_bytes = 0;    ///< Резидентная память процесса в байтах.
    unsigned long long shared_bytes = 0; ///< Разделяемая память процесса в байтах.
    unsigned long long total_jiffies = 0; ///< Сумма user/system CPU-тиков.
    unsigned long long start_time = 0;   ///< Время старта процесса в тиках с момента загрузки.
    double cpu_percent = 0.0;            ///< Процент CPU за период между двумя снимками.
    double mem_percent = 0.0;            ///< Доля RSS от общей памяти системы.
};

}
