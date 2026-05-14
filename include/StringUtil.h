#pragma once

#include <string>

namespace custom_htop {

/// Возвращает копию строки без пробельных символов по краям.
std::string trim(std::string value);

/// Проверяет, начинается ли строка с указанного префикса.
bool starts_with(const std::string& value, const std::string& prefix);

/// Возвращает копию строки в нижнем регистре для ASCII/HTTP-имен.
std::string lower_copy(std::string value);

}
