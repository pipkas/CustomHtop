#pragma once

#include <string>

namespace custom_htop {

/// Читает весь файл в строку; при ошибке возвращает пустую строку.
std::string read_file(const std::string& path);

}
