#pragma once

namespace custom_htop {

/// Минимальный предыдущий снимок процесса для расчета CPU% между обновлениями.
struct ProcSample {
    unsigned long long total_jiffies = 0; ///< Накопленные CPU-тики процесса.
    unsigned long long start_time = 0;    ///< Старт процесса для защиты от переиспользования PID.
};

}
