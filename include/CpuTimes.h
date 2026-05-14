#pragma once

namespace custom_htop {

/// Накопительные CPU-счетчики Linux по одному ядру из /proc/stat.
struct CpuTimes {
    int id = -1;                       ///< Номер CPU-ядра; -1 не используется в UI.
    unsigned long long user = 0;       ///< Время пользовательских процессов.
    unsigned long long nice = 0;       ///< Время процессов с измененным nice.
    unsigned long long system = 0;     ///< Время ядра.
    unsigned long long idle = 0;       ///< Время простоя.
    unsigned long long iowait = 0;     ///< Ожидание операций ввода-вывода.
    unsigned long long irq = 0;        ///< Обработка аппаратных прерываний.
    unsigned long long softirq = 0;    ///< Обработка программных прерываний.
    unsigned long long steal = 0;      ///< Время, украденное гипервизором.

    /// Возвращает сумму всех учитываемых счетчиков.
    unsigned long long total() const;

    /// Возвращает idle + iowait для расчета активной загрузки.
    unsigned long long idle_all() const;
};

}
