#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace mycross {

    // Win32 window class names and app-internal message ids.
    constexpr wchar_t OV_CLASS[] = L"MyCrossOverlay";
    constexpr wchar_t CTL_CLASS[] = L"MyCrossCtl";
    constexpr UINT WM_APP_SYNC = WM_APP + 1;
    constexpr UINT WM_APP_EXIT = WM_APP + 2;

    // Global hotkey: Ctrl+Alt+Shift+F12.
    constexpr int HOTKEY_ID = 1;
    constexpr UINT HOTKEY_MOD = MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT;
    constexpr UINT HOTKEY_VK = VK_F12;

    // Crosshair configuration: position, size, line width, color.
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

    // Shared runtime state, guarded by a mutex.
    struct State {
        std::mutex mu;
        Config cfg;
        bool running = false;
        std::wstring active = L"default.ini";
    };

    // Process-wide app context shared across the lifetime of the program.
    struct AppContext {
        State state;
        std::atomic<bool> exit{false};
        HINSTANCE inst = nullptr;
        std::wstring exe_dir;
        std::wstring cfg_dir;
        std::wstring web_dir;
        std::thread overlay_thread;
        std::atomic<bool> overlay_ready{false};
        HWND ctl_wnd = nullptr;
        HWND overlay_wnd = nullptr;
        HWND ui_wnd = nullptr;
        DWORD launch_error = 0;
    };

} // namespace mycross
