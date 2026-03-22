#include "server.h"

#include <cctype>
#include <fstream>
#include <map>
#include <sstream>

#include "common.h"
#include "config_store.h"
#include "overlay.h"

namespace mycross {
    namespace {

        std::string read_file(const std::wstring &path) {
            std::ifstream in(path.c_str(), std::ios::binary);
            if (!in) {
                return "";
            }
            std::ostringstream buffer;
            buffer << in.rdbuf();
            return buffer.str();
        }

        std::string urld(const std::string &in) {
            std::string out;
            for (size_t i = 0; i < in.size(); ++i) {
                if (in[i] == '%' && i + 2 < in.size()) {
                    out.push_back(static_cast<char>(
                        strtol(in.substr(i + 1, 2).c_str(), nullptr, 16)));
                    i += 2;
                } else if (in[i] == '+') {
                    out.push_back(' ');
                } else {
                    out.push_back(in[i]);
                }
            }
            return out;
        }

        std::map<std::string, std::string> form_parse(const std::string &body) {
            std::map<std::string, std::string> form;
            size_t start = 0;
            while (start <= body.size()) {
                const size_t amp = body.find('&', start);
                const std::string pair =
                    body.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
                if (!pair.empty()) {
                    const size_t eq = pair.find('=');
                    if (eq == std::string::npos) {
                        form[urld(pair)] = "";
                    } else {
                        form[urld(pair.substr(0, eq))] = urld(pair.substr(eq + 1));
                    }
                }
                if (amp == std::string::npos) {
                    break;
                }
                start = amp + 1;
            }
            return form;
        }

        void send_http(SOCKET client, int code, const char *status, const char *content_type,
                       const std::string &body) {
            std::ostringstream response;
            response << "HTTP/1.1 " << code << ' ' << status << "\r\n"
                     << "Content-Type: " << content_type << "\r\n"
                     << "Content-Length: " << body.size()
                     << "\r\nConnection: close\r\nCache-Control: no-cache\r\n\r\n"
                     << body;
            const std::string raw = response.str();
            send(client, raw.c_str(), static_cast<int>(raw.size()), 0);
        }

        bool read_req(SOCKET client, std::string &raw) {
            raw.clear();
            char buffer[4096];
            bool has_headers = false;
            size_t header_end = std::string::npos;
            size_t content_length = 0;

            while (true) {
                const int count = recv(client, buffer, sizeof(buffer), 0);
                if (count <= 0) {
                    return false;
                }
                raw.append(buffer, buffer + count);
                if (!has_headers) {
                    header_end = raw.find("\r\n\r\n");
                    if (header_end != std::string::npos) {
                        has_headers = true;
                        const std::string headers = raw.substr(0, header_end + 4);
                        size_t pos = headers.find("Content-Length:");
                        if (pos != std::string::npos) {
                            pos += 15;
                            while (pos < headers.size() &&
                                   (headers[pos] == ' ' || headers[pos] == '\t')) {
                                ++pos;
                            }
                            size_t end = pos;
                            while (end < headers.size() &&
                                   isdigit(static_cast<unsigned char>(headers[end]))) {
                                ++end;
                            }
                            content_length =
                                static_cast<size_t>(strtoul(headers.substr(pos, end - pos).c_str(),
                                                            nullptr, 10));
                        }
                    }
                }
                if (has_headers) {
                    const size_t body_offset = header_end + 4;
                    if (raw.size() - body_offset >= content_length) {
                        return true;
                    }
                }
            }
        }

        Config cfg_from_form(const std::map<std::string, std::string> &form,
                             const Config &original) {
            Config cfg = original;
            auto get = [&](const char *key, int fallback) {
                auto it = form.find(key);
                return it == form.end() ? fallback : toint(it->second, fallback);
            };
            cfg.x = get("x", cfg.x);
            cfg.y = get("y", cfg.y);
            cfg.window_size = get("window_size", cfg.window_size);
            cfg.cross_half = get("cross_half", cfg.cross_half);
            cfg.line_width = get("line_width", cfg.line_width);
            cfg.color_r = get("color_r", cfg.color_r);
            cfg.color_g = get("color_g", cfg.color_g);
            cfg.color_b = get("color_b", cfg.color_b);
            normalize(cfg);
            return cfg;
        }

        std::string state_json(AppContext &app) {
            Config cfg;
            bool running = false;
            std::wstring active;
            {
                std::lock_guard<std::mutex> lock(app.state.mu);
                cfg = app.state.cfg;
                running = app.state.running;
                active = app.state.active;
            }

            const auto profile_list = profiles(app);
            std::ostringstream json;
            json << "{"
                 << "\"running\":" << (running ? "true" : "false") << ","
                 << "\"active_profile\":\"" << jesc(wutf8(active)) << "\","
                 << "\"hotkey\":\"Ctrl+Alt+Shift+F12\","
                 << "\"config\":{"
                 << "\"x\":" << cfg.x << ","
                 << "\"y\":" << cfg.y << ","
                 << "\"window_size\":" << cfg.window_size << ","
                 << "\"cross_half\":" << cfg.cross_half << ","
                 << "\"line_width\":" << cfg.line_width << ","
                 << "\"color_r\":" << cfg.color_r << ","
                 << "\"color_g\":" << cfg.color_g << ","
                 << "\"color_b\":" << cfg.color_b << "},"
                 << "\"profiles\":[";
            for (size_t i = 0; i < profile_list.size(); ++i) {
                if (i > 0) {
                    json << ',';
                }
                json << "\"" << jesc(wutf8(profile_list[i])) << "\"";
            }
            json << "]}";
            return json.str();
        }

        bool api(AppContext &app, SOCKET client, const std::string &method,
                 const std::string &path, const std::string &body) {
            app.last_ping = GetTickCount();

            if (method == "GET" && path == "/api/state") {
                send_http(client, 200, "OK", "application/json; charset=utf-8", state_json(app));
                return true;
            }
            if (method == "POST" && path == "/api/ping") {
                send_http(client, 200, "OK", "text/plain; charset=utf-8", "pong");
                return true;
            }
            if (method == "POST" && path == "/api/toggle") {
                const auto form = form_parse(body);
                const bool running =
                    form.find("running") != form.end() && form.at("running") == "1";
                {
                    std::lock_guard<std::mutex> lock(app.state.mu);
                    app.state.running = running;
                }
                post_sync(app);
                send_http(client, 200, "OK", "application/json; charset=utf-8", state_json(app));
                return true;
            }
            if (method == "POST" && path == "/api/apply") {
                const auto form = form_parse(body);
                {
                    std::lock_guard<std::mutex> lock(app.state.mu);
                    app.state.cfg = cfg_from_form(form, app.state.cfg);
                }
                post_sync(app);
                send_http(client, 200, "OK", "application/json; charset=utf-8", state_json(app));
                return true;
            }
            if (method == "POST" && path == "/api/profile/load") {
                const auto form = form_parse(body);
                const auto it = form.find("name");
                if (it == form.end()) {
                    send_http(client, 400, "Bad Request", "text/plain; charset=utf-8",
                              "missing name");
                    return true;
                }
                const auto name = profile_name(utf8w(it->second));
                if (name.empty()) {
                    send_http(client, 400, "Bad Request", "text/plain; charset=utf-8",
                              "invalid name");
                    return true;
                }
                const auto file = profile_path(app, name);
                if (GetFileAttributesW(file.c_str()) == INVALID_FILE_ATTRIBUTES) {
                    send_http(client, 404, "Not Found", "text/plain; charset=utf-8",
                              "profile not found");
                    return true;
                }
                const Config loaded = load_cfg(file);
                {
                    std::lock_guard<std::mutex> lock(app.state.mu);
                    app.state.cfg = loaded;
                    app.state.active = name;
                }
                post_sync(app);
                send_http(client, 200, "OK", "application/json; charset=utf-8", state_json(app));
                return true;
            }
            if (method == "POST" && path == "/api/profile/save") {
                const auto form = form_parse(body);
                std::wstring name;
                {
                    std::lock_guard<std::mutex> lock(app.state.mu);
                    name = app.state.active;
                }
                const auto it = form.find("name");
                if (it != form.end()) {
                    const auto next_name = profile_name(utf8w(it->second));
                    if (!next_name.empty()) {
                        name = next_name;
                    }
                }
                if (name.empty()) {
                    send_http(client, 400, "Bad Request", "text/plain; charset=utf-8",
                              "invalid profile name");
                    return true;
                }
                Config out;
                {
                    std::lock_guard<std::mutex> lock(app.state.mu);
                    app.state.cfg = cfg_from_form(form, app.state.cfg);
                    out = app.state.cfg;
                    app.state.active = name;
                }
                if (!save_cfg(profile_path(app, name), out)) {
                    send_http(client, 500, "Internal Server Error", "text/plain; charset=utf-8",
                              "save failed");
                    return true;
                }
                post_sync(app);
                send_http(client, 200, "OK", "application/json; charset=utf-8", state_json(app));
                return true;
            }
            if (method == "POST" && path == "/api/profile/new") {
                const auto form = form_parse(body);
                const auto it = form.find("name");
                if (it == form.end()) {
                    send_http(client, 400, "Bad Request", "text/plain; charset=utf-8",
                              "missing name");
                    return true;
                }
                const auto name = profile_name(utf8w(it->second));
                if (name.empty()) {
                    send_http(client, 400, "Bad Request", "text/plain; charset=utf-8",
                              "invalid profile name");
                    return true;
                }
                const auto file = profile_path(app, name);
                if (GetFileAttributesW(file.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    send_http(client, 409, "Conflict", "text/plain; charset=utf-8",
                              "profile exists");
                    return true;
                }
                Config out;
                {
                    std::lock_guard<std::mutex> lock(app.state.mu);
                    app.state.cfg = cfg_from_form(form, app.state.cfg);
                    out = app.state.cfg;
                    app.state.active = name;
                }
                if (!save_cfg(file, out)) {
                    send_http(client, 500, "Internal Server Error", "text/plain; charset=utf-8",
                              "create failed");
                    return true;
                }
                post_sync(app);
                send_http(client, 200, "OK", "application/json; charset=utf-8", state_json(app));
                return true;
            }
            if (method == "POST" && path == "/api/profile/rename") {
                const auto form = form_parse(body);
                const auto it_new = form.find("new_name");
                if (it_new == form.end()) {
                    send_http(client, 400, "Bad Request", "text/plain; charset=utf-8",
                              "missing new_name");
                    return true;
                }

                std::wstring old_name;
                const auto it_old = form.find("old_name");
                if (it_old != form.end()) {
                    old_name = profile_name(utf8w(it_old->second));
                }
                if (old_name.empty()) {
                    std::lock_guard<std::mutex> lock(app.state.mu);
                    old_name = app.state.active;
                }

                const auto new_name = profile_name(utf8w(it_new->second));
                if (old_name.empty() || new_name.empty()) {
                    send_http(client, 400, "Bad Request", "text/plain; charset=utf-8",
                              "invalid name");
                    return true;
                }
                if (_wcsicmp(old_name.c_str(), new_name.c_str()) == 0) {
                    send_http(client, 200, "OK", "application/json; charset=utf-8", state_json(app));
                    return true;
                }

                const auto old_file = profile_path(app, old_name);
                const auto new_file = profile_path(app, new_name);
                if (GetFileAttributesW(old_file.c_str()) == INVALID_FILE_ATTRIBUTES) {
                    send_http(client, 404, "Not Found", "text/plain; charset=utf-8",
                              "source profile not found");
                    return true;
                }
                if (GetFileAttributesW(new_file.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    send_http(client, 409, "Conflict", "text/plain; charset=utf-8",
                              "target exists");
                    return true;
                }
                if (!MoveFileW(old_file.c_str(), new_file.c_str())) {
                    send_http(client, 500, "Internal Server Error", "text/plain; charset=utf-8",
                              "rename failed");
                    return true;
                }
                {
                    std::lock_guard<std::mutex> lock(app.state.mu);
                    if (_wcsicmp(app.state.active.c_str(), old_name.c_str()) == 0) {
                        app.state.active = new_name;
                    }
                }
                send_http(client, 200, "OK", "application/json; charset=utf-8", state_json(app));
                return true;
            }
            if (method == "POST" && path == "/api/quit") {
                app.exit = true;
                send_http(client, 200, "OK", "text/plain; charset=utf-8", "bye");
                return true;
            }
            return false;
        }

    } // namespace

    void handle_client(AppContext &app, SOCKET client) {
        std::string raw;
        if (!read_req(client, raw)) {
            closesocket(client);
            return;
        }

        const size_t header_end = raw.find("\r\n\r\n");
        if (header_end == std::string::npos) {
            send_http(client, 400, "Bad Request", "text/plain; charset=utf-8", "bad request");
            closesocket(client);
            return;
        }

        std::string request_line;
        {
            std::istringstream headers(raw.substr(0, header_end));
            std::getline(headers, request_line);
            if (!request_line.empty() && request_line.back() == '\r') {
                request_line.pop_back();
            }
        }

        std::string method;
        std::string path;
        std::string version;
        {
            std::istringstream line(request_line);
            line >> method >> path >> version;
        }
        const size_t query = path.find('?');
        if (query != std::string::npos) {
            path = path.substr(0, query);
        }
        const std::string body = raw.substr(header_end + 4);

        if (api(app, client, method, path, body)) {
            closesocket(client);
            return;
        }

        if (method == "GET" && path == "/") {
            const auto html = read_file(app.web_dir + L"\\index.html");
            if (html.empty()) {
                send_http(client, 500, "Internal Server Error", "text/plain; charset=utf-8",
                          "index.html missing");
            } else {
                send_http(client, 200, "OK", "text/html; charset=utf-8", html);
            }
            closesocket(client);
            return;
        }

        send_http(client, 404, "Not Found", "text/plain; charset=utf-8", "not found");
        closesocket(client);
    }

    bool start_server(AppContext &app) {
        WSADATA winsock_data = {};
        if (WSAStartup(MAKEWORD(2, 2), &winsock_data) != 0) {
            return false;
        }

        app.listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (app.listen_socket == INVALID_SOCKET) {
            WSACleanup();
            return false;
        }

        int reuse = 1;
        setsockopt(app.listen_socket, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char *>(&reuse), sizeof(reuse));

        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(PORT);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        if (bind(app.listen_socket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) !=
            0) {
            closesocket(app.listen_socket);
            app.listen_socket = INVALID_SOCKET;
            WSACleanup();
            return false;
        }

        if (listen(app.listen_socket, 16) != 0) {
            closesocket(app.listen_socket);
            app.listen_socket = INVALID_SOCKET;
            WSACleanup();
            return false;
        }

        u_long non_blocking = 1;
        ioctlsocket(app.listen_socket, FIONBIO, &non_blocking);
        return true;
    }

    void stop_server(AppContext &app) {
        if (app.listen_socket != INVALID_SOCKET) {
            closesocket(app.listen_socket);
            app.listen_socket = INVALID_SOCKET;
        }
        WSACleanup();
    }

    bool wait_server(int ms) {
        const DWORD start = GetTickCount();
        while (static_cast<int>(GetTickCount() - start) < ms) {
            SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (client == INVALID_SOCKET) {
                Sleep(50);
                continue;
            }
            sockaddr_in addr = {};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(PORT);
            inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
            const int rc = connect(client, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
            closesocket(client);
            if (rc == 0) {
                return true;
            }
            Sleep(60);
        }
        return false;
    }

} // namespace mycross
