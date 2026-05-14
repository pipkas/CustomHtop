#pragma once

#include <string>

namespace custom_htop {

/// Форматирует процессорное время процесса в стиле htop: H:MM:SS.cc.
std::string format_time_plus(unsigned long long jiffies, long ticks_per_second);

}
