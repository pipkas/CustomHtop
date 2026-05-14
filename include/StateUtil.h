#pragma once

#include <string>

namespace custom_htop {

/// Возвращает человекочитаемое имя состояния процесса Linux по символу из /proc/[pid]/stat.
std::string state_name(char state);

}
