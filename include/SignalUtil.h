#pragma once

#include <string>

namespace custom_htop {

/// Возвращает стандартное имя POSIX/Linux сигнала или стабильное fallback-имя.
std::string signal_name(int signum);

}
