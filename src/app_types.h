#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#include <winsock2.h>
#include <ws2tcpip.h>
#define WIN32_LEAN_AND_MEAN
#include <shellapi.h>
#include <windows.h>

namespace mycross {

    constexpr wchar_t OV_CLASS[] = L"MyCrossOverlay";
    constexpr wchar_t CTL_CLASS[] = L"MyCrossCtl";
    constexpr UINT WM_APP_SYNC = WM_APP + 1;
    constexpr UINT WM_APP_EXIT = WM_APP + 2;
    constexpr int PORT = 5188;
    constexpr DWORD HEARTBEAT_TIMEOUT_MS = 15000;
    constexpr int HOTKEY_ID = 1;
    constexpr UINT HOTKEY_MOD = MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT;
    constexpr UINT HOTKEY_VK = VK_F12;

    struct Config {
        int x = -1;
        int y = -1;
        int window_size = 40;
        int cross_half = 10;
        int line_width = 2;
        int color_r = 0;
        int color_g = 255;
        int color_b = 0;
    };

    struct State {
        std::mutex mu;
        Config cfg;
        bool running = false;
        std::wstring active = L"default.ini";
    };

    struct AppContext {
        State state;
        std::atomic<bool> exit{false};
        std::atomic<DWORD> last_ping{0};
        HINSTANCE inst = nullptr;
        std::wstring exe_dir;
        std::wstring cfg_dir;
        std::wstring web_dir;
        std::thread overlay_thread;
        std::atomic<bool> overlay_ready{false};
        HWND ctl_wnd = nullptr;
        HWND overlay_wnd = nullptr;
        SOCKET listen_socket = INVALID_SOCKET;
        bool app_mode = false;
        PROCESS_INFORMATION ui_proc = {};
        DWORD launch_error = 0;
    };

} // namespace mycross
