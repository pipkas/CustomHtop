#include "HttpServer.h"

#include <cstdlib>
#include <iostream>
#include <string>

/// Точка входа: выбирает порт из PORT/argv и запускает веб-сервер CustomHtop.
int main(int argc, char** argv) {
    int port = 8080;

    // PORT удобен для запуска из сервисов и контейнеров.
    if (const char* env_port = std::getenv("PORT")) {
        try {
            port = std::stoi(env_port);
        } catch (...) {
            std::cerr << "Invalid PORT, using 8080\n";
        }
    }

    // Аргумент командной строки имеет приоритет над переменной окружения.
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {
            std::cerr << "Usage: " << argv[0] << " [port]\n";
            return 1;
        }
    }

    if (port <= 0 || port > 65535) {
        std::cerr << "Port must be in range 1..65535\n";
        return 1;
    }

    custom_htop::HttpServer server(port);
    return server.run();
}
