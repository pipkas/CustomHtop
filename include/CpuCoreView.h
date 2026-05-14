#pragma once

namespace custom_htop {

/// Процентная разбивка загрузки одного CPU-ядра для отрисовки полосы в UI.
struct CpuCoreView {
    int id = 0;              ///< Номер CPU-ядра.
    double total = 0.0;      ///< Общая активная загрузка без idle/iowait.
    double low = 0.0;        ///< Доля nice-времени.
    double high = 0.0;       ///< Доля user-времени.
    double kernel = 0.0;     ///< Доля system/irq/softirq времени.
    double background = 0.0; ///< Доля iowait/steal времени.
};

}
