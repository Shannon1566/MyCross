#include "ui_launcher.h"

#include <cstdlib>
#include <map>
#include <sstream>
#include <string>

#include <wrl.h>

#include <WebView2.h>

#include "common.h"
#include "config_store.h"
#include "overlay.h"
#include <shellapi.h>

namespace mycross {
    namespace {

        using Microsoft::WRL::Callback;
        using Microsoft::WRL::ComPtr;

        // WebView UI 宿主窗口类名与虚拟域名。
        constexpr wchar_t UI_CLASS[] = L"MyCrossWebViewHost";
        constexpr wchar_t UI_HOST[] = L"app.mycross.local";

        // UI 线程内共享状态。
        AppContext *g_app = nullptr;
        ComPtr<ICoreWebView2Controller> g_controller;
        ComPtr<ICoreWebView2> g_webview;
        bool g_com_init = false;
        bool g_com_owned = false;

        // URL 解码（支持 %XX 与 + 空格）。
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

        // 解析前端桥接消息（form-urlencoded）。
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

        // 根据前端传参更新配置并做归一化。
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

        // 输出前端面板需要的状态 JSON。
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

        // 统一触发应用退出流程。
        void request_exit(AppContext &app) {
            app.exit = true;
            PostQuitMessage(0);
        }

        // 构造 bridge 成功响应。
        std::string ok_response(const std::string &id, const std::string &result_json) {
            std::ostringstream json;
            json << "{"
                 << "\"id\":\"" << jesc(id) << "\","
                 << "\"ok\":true,"
                 << "\"result\":" << result_json << "}";
            return json.str();
        }

        // 构造 bridge 错误响应。
        std::string err_response(const std::string &id, const std::string &code,
                                 const std::string &message) {
            std::ostringstream json;
            json << "{"
                 << "\"id\":\"" << jesc(id) << "\","
                 << "\"ok\":false,"
                 << "\"error\":{"
                 << "\"code\":\"" << jesc(code) << "\","
                 << "\"message\":\"" << jesc(message) << "\""
                 << "}}";
            return json.str();
        }

        // 处理 WebView bridge 调用并返回 JSON 字符串。
        std::string handle_bridge_call(AppContext &app,
                                       const std::map<std::string, std::string> &form) {
            const auto it_id = form.find("id");
            const auto it_method = form.find("method");
            const std::string id = it_id == form.end() ? "" : it_id->second;
            if (it_method == form.end() || it_method->second.empty()) {
                return err_response(id, "bad_request", "missing method");
            }

            const std::string &method = it_method->second;
            if (method == "state.get") {
                return ok_response(id, state_json(app));
            }
            if (method == "overlay.set_running") {
                const bool running =
                    form.find("running") != form.end() && form.at("running") == "1";
                {
                    std::lock_guard<std::mutex> lock(app.state.mu);
                    app.state.running = running;
                }
                post_sync(app);
                return ok_response(id, state_json(app));
            }
            if (method == "config.apply") {
                {
                    std::lock_guard<std::mutex> lock(app.state.mu);
                    app.state.cfg = cfg_from_form(form, app.state.cfg);
                }
                post_sync(app);
                return ok_response(id, state_json(app));
            }
            if (method == "profile.load") {
                const auto it = form.find("name");
                if (it == form.end()) {
                    return err_response(id, "bad_request", "missing name");
                }
                const auto name = profile_name(utf8w(it->second));
                if (name.empty()) {
                    return err_response(id, "bad_request", "invalid name");
                }
                const auto file = profile_path(app, name);
                if (GetFileAttributesW(file.c_str()) == INVALID_FILE_ATTRIBUTES) {
                    return err_response(id, "not_found", "profile not found");
                }
                const Config loaded = load_cfg(file);
                {
                    std::lock_guard<std::mutex> lock(app.state.mu);
                    app.state.cfg = loaded;
                    app.state.active = name;
                }
                post_sync(app);
                return ok_response(id, state_json(app));
            }
            if (method == "profile.save") {
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
                    return err_response(id, "bad_request", "invalid profile name");
                }

                Config out;
                {
                    std::lock_guard<std::mutex> lock(app.state.mu);
                    app.state.cfg = cfg_from_form(form, app.state.cfg);
                    out = app.state.cfg;
                    app.state.active = name;
                }
                if (!save_cfg(profile_path(app, name), out)) {
                    return err_response(id, "io_error", "save failed");
                }
                post_sync(app);
                return ok_response(id, state_json(app));
            }
            if (method == "profile.create") {
                const auto it = form.find("name");
                if (it == form.end()) {
                    return err_response(id, "bad_request", "missing name");
                }
                const auto name = profile_name(utf8w(it->second));
                if (name.empty()) {
                    return err_response(id, "bad_request", "invalid profile name");
                }
                const auto file = profile_path(app, name);
                if (GetFileAttributesW(file.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    return err_response(id, "conflict", "profile exists");
                }

                Config out;
                {
                    std::lock_guard<std::mutex> lock(app.state.mu);
                    app.state.cfg = cfg_from_form(form, app.state.cfg);
                    out = app.state.cfg;
                    app.state.active = name;
                }
                if (!save_cfg(file, out)) {
                    return err_response(id, "io_error", "create failed");
                }
                post_sync(app);
                return ok_response(id, state_json(app));
            }
            if (method == "profile.rename") {
                const auto it_new = form.find("new_name");
                if (it_new == form.end()) {
                    return err_response(id, "bad_request", "missing new_name");
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
                    return err_response(id, "bad_request", "invalid name");
                }
                if (_wcsicmp(old_name.c_str(), new_name.c_str()) == 0) {
                    return ok_response(id, state_json(app));
                }

                const auto old_file = profile_path(app, old_name);
                const auto new_file = profile_path(app, new_name);
                if (GetFileAttributesW(old_file.c_str()) == INVALID_FILE_ATTRIBUTES) {
                    return err_response(id, "not_found", "source profile not found");
                }
                if (GetFileAttributesW(new_file.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    return err_response(id, "conflict", "target exists");
                }
                if (!MoveFileW(old_file.c_str(), new_file.c_str())) {
                    return err_response(id, "io_error", "rename failed");
                }
                {
                    std::lock_guard<std::mutex> lock(app.state.mu);
                    if (_wcsicmp(app.state.active.c_str(), old_name.c_str()) == 0) {
                        app.state.active = new_name;
                    }
                }
                return ok_response(id, state_json(app));
            }
            if (method == "app.quit") {
                request_exit(app);
                return ok_response(id, "{}");
            }
            return err_response(id, "not_implemented", "unknown method");
        }

        // 跟随宿主窗口尺寸刷新 WebView 视图区域。
        void set_webview_bounds() {
            if (!g_controller || !g_app || !g_app->ui_wnd) {
                return;
            }
            RECT bounds = {};
            GetClientRect(g_app->ui_wnd, &bounds);
            g_controller->put_Bounds(bounds);
        }

        // UI 宿主窗口过程。
        LRESULT CALLBACK UiProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
            switch (message) {
            case WM_SIZE:
                set_webview_bounds();
                return 0;
            case WM_CLOSE:
                if (g_app != nullptr) {
                    request_exit(*g_app);
                }
                DestroyWindow(hwnd);
                return 0;
            case WM_DESTROY:
                if (g_app != nullptr && g_app->ui_wnd == hwnd) {
                    g_app->ui_wnd = nullptr;
                }
                PostQuitMessage(0);
                return 0;
            default:
                return DefWindowProcW(hwnd, message, wparam, lparam);
            }
        }

        // 异步初始化 WebView2，并注册消息桥接。
        bool init_webview(AppContext &app) {
            bool done = false;
            bool ok = false;
            HRESULT init_hr = S_OK;

            const std::wstring host_dir = app.web_dir;

            const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
                nullptr, nullptr, nullptr,
                Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                    [&](HRESULT result, ICoreWebView2Environment *env) -> HRESULT {
                        if (FAILED(result) || env == nullptr) {
                            init_hr = FAILED(result) ? result : E_FAIL;
                            done = true;
                            return S_OK;
                        }

                        env->CreateCoreWebView2Controller(
                            app.ui_wnd,
                            Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                                [&](HRESULT result2, ICoreWebView2Controller *controller)
                                    -> HRESULT {
                                    if (FAILED(result2) || controller == nullptr) {
                                        init_hr = FAILED(result2) ? result2 : E_FAIL;
                                        done = true;
                                        return S_OK;
                                    }

                                    g_controller = controller;
                                    g_controller->get_CoreWebView2(&g_webview);
                                    if (!g_webview) {
                                        init_hr = E_FAIL;
                                        done = true;
                                        return S_OK;
                                    }

                                    set_webview_bounds();
                                    ComPtr<ICoreWebView2_3> webview3;
                                    if (FAILED(g_webview.As(&webview3)) || !webview3) {
                                        init_hr = E_NOINTERFACE;
                                        done = true;
                                        return S_OK;
                                    }
                                    webview3->SetVirtualHostNameToFolderMapping(
                                        UI_HOST, host_dir.c_str(),
                                        COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);

                                    EventRegistrationToken token = {};
                                    g_webview->add_WebMessageReceived(
                                        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                            [&](ICoreWebView2 *,
                                                ICoreWebView2WebMessageReceivedEventArgs *args)
                                                -> HRESULT {
                                                LPWSTR msg = nullptr;
                                                if (FAILED(args->TryGetWebMessageAsString(&msg)) ||
                                                    msg == nullptr || g_app == nullptr ||
                                                    !g_webview) {
                                                    if (msg != nullptr) {
                                                        CoTaskMemFree(msg);
                                                    }
                                                    return S_OK;
                                                }
                                                const std::string request = wutf8(msg);
                                                CoTaskMemFree(msg);
                                                const auto form = form_parse(request);
                                                const std::string response =
                                                    handle_bridge_call(*g_app, form);
                                                const std::wstring response_w =
                                                    utf8w(response);
                                                g_webview->PostWebMessageAsJson(
                                                    response_w.c_str());
                                                return S_OK;
                                            })
                                            .Get(),
                                        &token);

                                    g_webview->Navigate(L"https://app.mycross.local/index.html");
                                    ok = true;
                                    done = true;
                                    return S_OK;
                                })
                                .Get());
                        return S_OK;
                    })
                    .Get());

            if (FAILED(hr)) {
                app.launch_error = static_cast<DWORD>(hr);
                return false;
            }

            while (!done) {
                MSG msg = {};
                while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
                Sleep(10);
            }

            if (!ok) {
                app.launch_error = static_cast<DWORD>(init_hr);
                return false;
            }

            return true;
        }

    } // namespace

    // 初始化 COM、创建宿主窗口并启动 WebView2。
    bool launch_ui(AppContext &app) {
        app.launch_error = 0;

        if (!g_com_init) {
            const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
                app.launch_error = static_cast<DWORD>(hr);
                return false;
            }
            g_com_init = true;
            g_com_owned = (hr == S_OK || hr == S_FALSE);
        }

        g_app = &app;

        WNDCLASSW wc = {};
        wc.lpfnWndProc = UiProc;
        wc.hInstance = app.inst;
        wc.lpszClassName = UI_CLASS;
        wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        RegisterClassW(&wc);

        app.ui_wnd = CreateWindowExW(0, UI_CLASS, L"MyCross", WS_OVERLAPPEDWINDOW,
                                     CW_USEDEFAULT, CW_USEDEFAULT, 1120, 700, nullptr, nullptr,
                                     app.inst, nullptr);
        if (!app.ui_wnd) {
            app.launch_error = GetLastError();
            return false;
        }

        ShowWindow(app.ui_wnd, SW_SHOWNORMAL);
        UpdateWindow(app.ui_wnd);

        if (!init_webview(app)) {
            DestroyWindow(app.ui_wnd);
            app.ui_wnd = nullptr;
            return false;
        }

        return true;
    }

    // 释放 WebView2/窗口/COM 资源。
    void stop_ui(AppContext &app) {
        g_webview.Reset();
        g_controller.Reset();

        if (app.ui_wnd) {
            DestroyWindow(app.ui_wnd);
            app.ui_wnd = nullptr;
        }

        if (g_com_owned) {
            CoUninitialize();
        }
        g_com_init = false;
        g_com_owned = false;
        g_app = nullptr;
    }

    // 支持位置参数与 --x/--y 两种命令行形式。
    void apply_cli(AppContext &app) {
        int argc = 0;
        LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (!argv) {
            return;
        }

        Config cfg;
        {
            std::lock_guard<std::mutex> lock(app.state.mu);
            cfg = app.state.cfg;
        }

        if (argc >= 3) {
            cfg.x = _wtoi(argv[1]);
            cfg.y = _wtoi(argv[2]);
        }
        for (int i = 1; i < argc; ++i) {
            if (wcsncmp(argv[i], L"--x=", 4) == 0) {
                cfg.x = _wtoi(argv[i] + 4);
            } else if (wcsncmp(argv[i], L"--y=", 4) == 0) {
                cfg.y = _wtoi(argv[i] + 4);
            }
        }

        normalize(cfg);
        {
            std::lock_guard<std::mutex> lock(app.state.mu);
            app.state.cfg = cfg;
        }
        LocalFree(argv);
    }

} // namespace mycross
