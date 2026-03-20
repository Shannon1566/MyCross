
#include <windows.h>
#include <shellapi.h>
#include <cstdlib>
#include <cstring>

namespace {
constexpr int kWindowSize = 40;
constexpr int kCrossHalf = 10;
constexpr int kLineWidth = 2;
constexpr int kHotkeyId = 1;

int g_screenX = -1;
int g_screenY = -1;

int ParseInt(const wchar_t* text, int fallback)
{
    if (text == nullptr || *text == L'\0') {
        return fallback;
    }

    wchar_t* end = nullptr;
    const long value = wcstol(text, &end, 10);
    if (end == text || *end != L'\0') {
        return fallback;
    }

    return static_cast<int>(value);
}

bool StartsWith(const wchar_t* text, const wchar_t* prefix)
{
    return text != nullptr && prefix != nullptr && wcsncmp(text, prefix, wcslen(prefix)) == 0;
}

void ClampToScreen(int& x, int& y, int screenW, int screenH)
{
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > screenW - 1) x = screenW - 1;
    if (y > screenH - 1) y = screenH - 1;
}

void ShowHelp()
{
    const wchar_t* message =
        L"MyCross Usage:\n"
        L"  crosshair.exe                -> center of screen\n"
        L"  crosshair.exe <x> <y>        -> place by coordinates\n"
        L"  crosshair.exe --x=<x> --y=<y>\n"
        L"\n"
        L"Press Esc to quit.\n";

    MessageBoxW(nullptr, message, L"MyCross Help", MB_OK | MB_ICONINFORMATION);
}

void ParseArguments()
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr) {
        return;
    }

    // Supported forms:
    // 1) crosshair.exe x y
    // 2) crosshair.exe --x=100 --y=200
    // 3) crosshair.exe --help
    if (argc >= 2 && (wcscmp(argv[1], L"--help") == 0 || wcscmp(argv[1], L"-h") == 0)) {
        ShowHelp();
        LocalFree(argv);
        ExitProcess(0);
    }

    if (argc >= 3) {
        g_screenX = ParseInt(argv[1], g_screenX);
        g_screenY = ParseInt(argv[2], g_screenY);
    }

    for (int i = 1; i < argc; ++i) {
        if (StartsWith(argv[i], L"--x=")) {
            g_screenX = ParseInt(argv[i] + 4, g_screenX);
        } else if (StartsWith(argv[i], L"--y=")) {
            g_screenY = ParseInt(argv[i] + 4, g_screenY);
        }
    }

    LocalFree(argv);
}
}  // namespace

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        HPEN pen = CreatePen(PS_SOLID, kLineWidth, RGB(0, 255, 0));
        HGDIOBJ oldPen = SelectObject(hdc, pen);

        const int cx = kWindowSize / 2;
        const int cy = kWindowSize / 2;

        MoveToEx(hdc, cx - kCrossHalf, cy, nullptr);
        LineTo(hdc, cx + kCrossHalf, cy);

        MoveToEx(hdc, cx, cy - kCrossHalf, nullptr);
        LineTo(hdc, cx, cy + kCrossHalf);

        SelectObject(hdc, oldPen);
        DeleteObject(pen);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_HOTKEY:
        if (wParam == kHotkeyId) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_DESTROY:
        UnregisterHotKey(hwnd, kHotkeyId);
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    ParseArguments();

    const int screenW = GetSystemMetrics(SM_CXSCREEN);
    const int screenH = GetSystemMetrics(SM_CYSCREEN);

    if (g_screenX < 0) g_screenX = screenW / 2;
    if (g_screenY < 0) g_screenY = screenH / 2;

    ClampToScreen(g_screenX, g_screenY, screenW, screenH);

    const int winX = g_screenX - kWindowSize / 2;
    const int winY = g_screenY - kWindowSize / 2;

    const wchar_t kClassName[] = L"MyCrossWindow";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kClassName;

    if (RegisterClassW(&wc) == 0) {
        return 1;
    }

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST |
            WS_EX_LAYERED |
            WS_EX_TRANSPARENT |
            WS_EX_NOACTIVATE,
        kClassName,
        L"",
        WS_POPUP,
        winX,
        winY,
        kWindowSize,
        kWindowSize,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (hwnd == nullptr) {
        return 1;
    }

    // Make black background fully transparent.
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

    RegisterHotKey(hwnd, kHotkeyId, MOD_NOREPEAT, VK_ESCAPE);

    ShowWindow(hwnd, SW_SHOW);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
