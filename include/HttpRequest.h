#pragma once

#include <string>
#include <unordered_map>

namespace custom_htop {

/// Разобранные части одного HTTP-запроса, достаточные для роутинга API и статики.
struct HttpRequest {
    std::string method; ///< HTTP-метод, например GET или POST.
    std::string target; ///< Исходный target из первой строки запроса, включая query.
    std::string path; ///< Путь без query-параметров.
    std::string body; ///< Тело запроса после заголовков.
    std::unordered_map<std::string, std::string> headers; ///< Заголовки с именами в нижнем регистре.
};

}
