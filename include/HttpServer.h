#pragma once

#include "HttpRequest.h"
#include "ProcessSampler.h"

#include <string>

namespace custom_htop {

/// Минимальный многопоточный HTTP-сервер для статики WebUI и JSON API мониторинга.
class HttpServer {
public:
    /// Создает сервер, который будет слушать указанный TCP-порт.
    explicit HttpServer(int port);

    /// Запускает accept-loop; возвращает код ошибки только при сбое запуска сервера.
    int run();

private:
    /// Читает и разбирает один HTTP-запрос из сокета.
    bool read_request(int fd, HttpRequest& request);

    /// Отправляет HTTP-ответ с указанным статусом, MIME-типом и телом.
    void send_response(int fd, int status, const std::string& type, const std::string& body);

    /// Гарантированно досылает все байты строки в сокет, пока соединение активно.
    void send_all(int fd, const std::string& data);

    /// Возвращает текстовую причину HTTP-статуса.
    std::string status_reason(int status);

    /// Определяет MIME-тип для статического файла WebUI.
    std::string mime_type(const std::string& path);

    /// Преобразует HTTP-путь в путь внутри каталога web и отсекает traversal.
    std::string static_path_for(const std::string& request_path);

    /// Обрабатывает один клиентский сокет и закрывает его после ответа.
    void handle_client(int fd);

    /// Возвращает JSON со списком доступных Linux-сигналов.
    std::string signals_json();

    /// Валидирует JSON-тело и отправляет выбранный сигнал указанному PID.
    std::string signal_process(const std::string& body);

    int port_; ///< TCP-порт HTTP-сервера.
    ProcessSampler sampler_; ///< Источник системных снимков для API.
};

}
