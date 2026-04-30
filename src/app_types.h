#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace mycross {

    // Win32 窗口类名与应用内消息定义。
    constexpr wchar_t OV_CLASS[] = L"MyCrossOverlay";
    constexpr wchar_t CTL_CLASS[] = L"MyCrossCtl";
    constexpr UINT WM_APP_SYNC = WM_APP + 1;
    constexpr UINT WM_APP_EXIT = WM_APP + 2;
    // 全局快捷键：Ctrl+Alt+Shift+F12。
    constexpr int HOTKEY_ID = 1;
    constexpr UINT HOTKEY_MOD = MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT;
    constexpr UINT HOTKEY_VK = VK_F12;

    // 准星配置（位置、尺寸、线宽、颜色）。
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

    // 运行时共享状态（由互斥锁保护）。
    struct State {
        std::mutex mu;
        Config cfg;
        bool running = false;
        std::wstring active = L"default.ini";
    };

    // 应用全局上下文：生命周期内共享的资源与句柄。
    struct AppContext {
        State state;
        std::atomic<bool> exit{false};
        HINSTANCE inst = nullptr;
        std::wstring exe_dir;
        std::wstring cfg_dir;
        std::thread overlay_thread;
        std::atomic<bool> overlay_ready{false};
        HWND ctl_wnd = nullptr;
        HWND overlay_wnd = nullptr;
    };

} // namespace mycross
