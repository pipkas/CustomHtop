#include "HttpServer.h"

#include "FileUtil.h"
#include "JsonUtil.h"
#include "SignalUtil.h"
#include "StringUtil.h"

#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_set>

namespace {

bool query_bool(const std::string& target, const std::string& key) {
    const std::size_t query_pos = target.find('?');
    if (query_pos == std::string::npos) {
        return false;
    }

    std::size_t start = query_pos + 1;
    while (start <= target.size()) {
        const std::size_t end = target.find('&', start);
        const std::string item = target.substr(start, end == std::string::npos ? std::string::npos : end - start);
        const std::size_t equals = item.find('=');
        const std::string item_key = equals == std::string::npos ? item : item.substr(0, equals);
        const std::string item_value = equals == std::string::npos ? "" : item.substr(equals + 1);
        if (item_key == key) {
            return item_value.empty()
                || item_value == "1"
                || item_value == "true"
                || item_value == "on"
                || item_value == "yes";
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return false;
}

} // namespace

namespace custom_htop {

HttpServer::HttpServer(int port) : port_(port) {}

int HttpServer::run() {
    signal(SIGPIPE, SIG_IGN);

    const int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "socket: " << std::strerror(errno) << '\n';
        return 1;
    }

    int yes = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<uint16_t>(port_));

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        std::cerr << "bind: " << std::strerror(errno) << '\n';
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 64) < 0) {
        std::cerr << "listen: " << std::strerror(errno) << '\n';
        close(server_fd);
        return 1;
    }

    std::cout << "CustomHtop WebUI: http://127.0.0.1:" << port_ << "\n";
    std::cout << "Press Ctrl+C to stop.\n";

    while (true) {
        sockaddr_in client{};
        socklen_t client_len = sizeof(client);
        const int client_fd = accept(server_fd, reinterpret_cast<sockaddr*>(&client), &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "accept: " << std::strerror(errno) << '\n';
            continue;
        }
        std::thread(&HttpServer::handle_client, this, client_fd).detach();
    }
}

bool HttpServer::read_request(int fd, HttpRequest& request) {
    std::string data;
    char buffer[8192];
    const std::size_t max_request_size = 2 * 1024 * 1024;

    while (data.find("\r\n\r\n") == std::string::npos) {
        const ssize_t received = recv(fd, buffer, sizeof(buffer), 0);
        if (received <= 0) {
            return false;
        }
        data.append(buffer, static_cast<std::size_t>(received));
        if (data.size() > max_request_size) {
            return false;
        }
    }

    const std::size_t header_end = data.find("\r\n\r\n");
    const std::string header_text = data.substr(0, header_end);
    request.body = data.substr(header_end + 4);

    std::istringstream headers(header_text);
    std::string request_line;
    std::getline(headers, request_line);
    if (!request_line.empty() && request_line.back() == '\r') {
        request_line.pop_back();
    }

    std::istringstream first_line(request_line);
    first_line >> request.method >> request.target;
    if (request.method.empty() || request.target.empty()) {
        return false;
    }
    request.path = request.target;
    const std::size_t query = request.path.find('?');
    if (query != std::string::npos) {
        request.path = request.path.substr(0, query);
    }

    std::string line;
    while (std::getline(headers, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        request.headers[lower_copy(trim(line.substr(0, colon)))] = trim(line.substr(colon + 1));
    }

    std::size_t content_length = 0;
    const auto found_length = request.headers.find("content-length");
    if (found_length != request.headers.end()) {
        try {
            content_length = static_cast<std::size_t>(std::stoull(found_length->second));
        } catch (...) {
            return false;
        }
    }

    while (request.body.size() < content_length) {
        const ssize_t received = recv(fd, buffer, sizeof(buffer), 0);
        if (received <= 0) {
            return false;
        }
        request.body.append(buffer, static_cast<std::size_t>(received));
        if (request.body.size() > max_request_size) {
            return false;
        }
    }
    if (request.body.size() > content_length) {
        request.body.resize(content_length);
    }
    return true;
}

void HttpServer::send_response(int fd, int status, const std::string& type, const std::string& body) {
    const std::string reason = status_reason(status);
    std::ostringstream response;
    response << "HTTP/1.1 " << status << ' ' << reason << "\r\n"
             << "Content-Type: " << type << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Cache-Control: no-store\r\n"
             << "Connection: close\r\n\r\n";
    const std::string header = response.str();
    send_all(fd, header);
    send_all(fd, body);
}

void HttpServer::send_all(int fd, const std::string& data) {
    const char* ptr = data.data();
    std::size_t left = data.size();
    while (left > 0) {
        const ssize_t sent = send(fd, ptr, left, 0);
        if (sent <= 0) {
            return;
        }
        ptr += sent;
        left -= static_cast<std::size_t>(sent);
    }
}

std::string HttpServer::status_reason(int status) {
    switch (status) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default: return "OK";
    }
}

std::string HttpServer::mime_type(const std::string& path) {
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".html") {
        return "text/html; charset=utf-8";
    }
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".css") {
        return "text/css; charset=utf-8";
    }
    if (path.size() >= 3 && path.substr(path.size() - 3) == ".js") {
        return "application/javascript; charset=utf-8";
    }
    if (path.size() >= 3 && path.substr(path.size() - 3) == ".ts") {
        return "text/plain; charset=utf-8";
    }
    return "application/octet-stream";
}

std::string HttpServer::static_path_for(const std::string& request_path) {
    if (request_path == "/") {
        return "web/index.html";
    }
    if (request_path.find("..") != std::string::npos) {
        return {};
    }
    return "web" + request_path;
}

void HttpServer::handle_client(int fd) {
    HttpRequest request;
    if (!read_request(fd, request)) {
        send_response(fd, 400, "application/json; charset=utf-8", "{\"ok\":false,\"error\":\"Bad request\"}");
        close(fd);
        return;
    }

    if (request.path == "/api/snapshot") {
        if (request.method != "GET") {
            send_response(fd, 405, "application/json; charset=utf-8", "{\"ok\":false,\"error\":\"Method not allowed\"}");
        } else {
            send_response(fd, 200, "application/json; charset=utf-8", sampler_.snapshot_json(query_bool(request.target, "threads")));
        }
        close(fd);
        return;
    }

    if (request.path == "/api/signals") {
        if (request.method != "GET") {
            send_response(fd, 405, "application/json; charset=utf-8", "{\"ok\":false,\"error\":\"Method not allowed\"}");
        } else {
            send_response(fd, 200, "application/json; charset=utf-8", signals_json());
        }
        close(fd);
        return;
    }

    if (request.path == "/api/signal") {
        if (request.method != "POST") {
            send_response(fd, 405, "application/json; charset=utf-8", "{\"ok\":false,\"error\":\"Method not allowed\"}");
        } else {
            send_response(fd, 200, "application/json; charset=utf-8", signal_process(request.body));
        }
        close(fd);
        return;
    }

    if (request.method != "GET") {
        send_response(fd, 405, "application/json; charset=utf-8", "{\"ok\":false,\"error\":\"Method not allowed\"}");
        close(fd);
        return;
    }

    const std::string file_path = static_path_for(request.path);
    if (file_path.empty()) {
        send_response(fd, 404, "text/plain; charset=utf-8", "Not found");
        close(fd);
        return;
    }

    const std::string content = read_file(file_path);
    if (content.empty()) {
        send_response(fd, 404, "text/plain; charset=utf-8", "Not found");
    } else {
        send_response(fd, 200, mime_type(file_path), content);
    }
    close(fd);
}

std::string HttpServer::signals_json() {
    std::ostringstream out;
    out << "{\"signals\":[";
    bool first = true;
    std::unordered_set<int> emitted;
    for (int signum = 1; signum < NSIG; ++signum) {
        if (emitted.count(signum) > 0) {
            continue;
        }
        const char* description = strsignal(signum);
        if (!description) {
            continue;
        }

        if (!first) {
            out << ',';
        }
        first = false;
        emitted.insert(signum);
        out << "{\"number\":" << signum
            << ",\"name\":\"" << escape_json(signal_name(signum)) << '"'
            << ",\"description\":\"" << escape_json(description) << "\"}";
    }
    out << "]}";
    return out.str();
}

std::string HttpServer::signal_process(const std::string& body) {
    const auto pid_value = json_int_value(body, "pid");
    const auto signal_value = json_int_value(body, "signal");
    if (!pid_value.has_value() || !signal_value.has_value() || *pid_value <= 0) {
        return "{\"ok\":false,\"error\":\"Invalid pid or signal\"}";
    }

    const int pid = static_cast<int>(*pid_value);
    const int signum = static_cast<int>(*signal_value);
    if (signum <= 0 || signum >= NSIG) {
        return "{\"ok\":false,\"error\":\"Signal is outside Linux signal range\"}";
    }

    if (kill(pid, signum) == 0) {
        std::ostringstream out;
        out << "{\"ok\":true,\"pid\":" << pid
            << ",\"signal\":" << signum
            << ",\"signalName\":\"" << escape_json(signal_name(signum)) << "\"}";
        return out.str();
    }

    const int err = errno;
    std::ostringstream out;
    out << "{\"ok\":false,\"pid\":" << pid
        << ",\"signal\":" << signum
        << ",\"signalName\":\"" << escape_json(signal_name(signum)) << '"'
        << ",\"errno\":" << err
        << ",\"error\":\"" << escape_json(std::strerror(err)) << "\"}";
    return out.str();
}

}
