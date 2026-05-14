#pragma once

#include <optional>
#include <string>

namespace custom_htop {

/// Экранирует строку так, чтобы ее можно было безопасно вставить в JSON-значение.
std::string escape_json(const std::string& value);

/// Извлекает целочисленное значение по ключу из небольшого JSON-тела запроса.
std::optional<long long> json_int_value(const std::string& body, const std::string& key);

}
