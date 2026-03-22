#include "app_types.h"
#include "common.h"
#include "config_store.h"
#include "overlay.h"
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

    if (!mycross::launch_ui(app)) {
        wchar_t message[256] = {};
        swprintf(message, 256,
                 L"\u542f\u52a8 WebView2 \u7a97\u53e3\u5931\u8d25\uff08\u9519\u8bef\u7801: %lu\uff09\u3002",
                 app.launch_error);
        MessageBoxW(nullptr, message, L"MyCross", MB_OK | MB_ICONERROR);
        mycross::stop_overlay(app);
        return 1;
    }

    MSG msg = {};
    while (!app.exit.load()) {
        const BOOL rc = GetMessageW(&msg, nullptr, 0, 0);
        if (rc <= 0) {
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    mycross::stop_ui(app);
    mycross::stop_overlay(app);
    return 0;
}
