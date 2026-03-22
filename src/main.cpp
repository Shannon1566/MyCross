#include "app_types.h"
#include "common.h"
#include "config_store.h"
#include "overlay.h"
#include "server.h"
#include "ui_launcher.h"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    mycross::AppContext app;
    app.inst = hInst;
    app.exe_dir = mycross::exe_dir();
    app.cfg_dir = app.exe_dir + L"\\configs";
    app.web_dir = app.exe_dir + L"\\web";

    mycross::ensure_cfg(app);
    {
        std::lock_guard<std::mutex> lock(app.state.mu);
        app.state.active = L"default.ini";
        app.state.cfg = mycross::load_cfg(mycross::profile_path(app, app.state.active));
        app.state.running = false;
    }

    mycross::apply_cli(app);
    mycross::start_overlay(app);
    mycross::post_sync(app);

    if (!mycross::start_server(app)) {
        MessageBoxW(nullptr,
                    L"\u542f\u52a8 HTTP \u670d\u52a1\u5931\u8d25\uff08\u7aef\u53e3 5188\uff09\u3002",
                    L"MyCross", MB_OK | MB_ICONERROR);
        mycross::stop_overlay(app);
        return 1;
    }

    mycross::wait_server(3000);
    app.last_ping = GetTickCount();
    if (!mycross::launch_ui(app)) {
        wchar_t message[256] = {};
        swprintf(message, 256,
                 L"\u542f\u52a8\u5e94\u7528\u7a97\u53e3\u5931\u8d25\uff08\u9519\u8bef\u7801: %lu\uff09\uff0c\u5df2\u56de\u9000\u5230\u9ed8\u8ba4\u6d4f\u89c8\u5668\u3002",
                 app.launch_error);
        MessageBoxW(nullptr, message, L"MyCross", MB_OK | MB_ICONWARNING);
        ShellExecuteW(nullptr, L"open", L"http://127.0.0.1:5188/", nullptr, nullptr,
                      SW_SHOWNORMAL);
        app.app_mode = false;
    } else {
        app.app_mode = true;
    }

    while (!app.exit.load()) {
        if (app.app_mode) {
            const DWORD now = GetTickCount();
            const DWORD last = app.last_ping.load();
            if (now - last > mycross::HEARTBEAT_TIMEOUT_MS) {
                app.exit = true;
                break;
            }
        }

        sockaddr_in client_addr = {};
        int client_len = sizeof(client_addr);
        SOCKET client = accept(app.listen_socket,
                               reinterpret_cast<sockaddr *>(&client_addr), &client_len);
        if (client == INVALID_SOCKET) {
            const int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                Sleep(25);
                continue;
            }
            Sleep(25);
            continue;
        }
        mycross::handle_client(app, client);
    }

    mycross::stop_server(app);
    mycross::stop_ui(app);
    mycross::stop_overlay(app);
    return 0;
}
