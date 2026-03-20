#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
constexpr wchar_t kMainWindowClass[] = L"MyCrossMainWindow";
constexpr wchar_t kOverlayWindowClass[] = L"MyCrossOverlayWindow";

constexpr int kDefaultWindowSize = 40;
constexpr int kDefaultCrossHalf = 10;
constexpr int kDefaultLineWidth = 2;
constexpr int kDefaultColorR = 0;
constexpr int kDefaultColorG = 255;
constexpr int kDefaultColorB = 0;

constexpr int kMainWindowWidth = 520;
constexpr int kMainWindowHeight = 430;

constexpr int kHotkeyId = 1;
constexpr UINT kQuitHotkeyModifiers = MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT;
constexpr UINT kQuitHotkeyVirtualKey = VK_F12;

constexpr int IDC_PROFILE_COMBO = 1001;
constexpr int IDC_BUTTON_REFRESH = 1002;
constexpr int IDC_BUTTON_NEW = 1003;
constexpr int IDC_BUTTON_SAVE = 1004;
constexpr int IDC_BUTTON_TOGGLE = 1005;

constexpr int IDC_EDIT_X = 1101;
constexpr int IDC_EDIT_Y = 1102;
constexpr int IDC_EDIT_WINDOW_SIZE = 1103;
constexpr int IDC_EDIT_CROSS_HALF = 1104;
constexpr int IDC_EDIT_LINE_WIDTH = 1105;
constexpr int IDC_EDIT_R = 1106;
constexpr int IDC_EDIT_G = 1107;
constexpr int IDC_EDIT_B = 1108;

struct CrosshairConfig {
    int x = -1;
    int y = -1;
    int windowSize = kDefaultWindowSize;
    int crossHalf = kDefaultCrossHalf;
    int lineWidth = kDefaultLineWidth;
    int colorR = kDefaultColorR;
    int colorG = kDefaultColorG;
    int colorB = kDefaultColorB;
};

HINSTANCE g_instance = nullptr;
HWND g_mainWindow = nullptr;
HWND g_overlayWindow = nullptr;
HWND g_profileCombo = nullptr;
HWND g_toggleButton = nullptr;
HWND g_editX = nullptr;
HWND g_editY = nullptr;
HWND g_editWindowSize = nullptr;
HWND g_editCrossHalf = nullptr;
HWND g_editLineWidth = nullptr;
HWND g_editR = nullptr;
HWND g_editG = nullptr;
HWND g_editB = nullptr;

std::wstring g_exeDir;
std::wstring g_configDir;
std::wstring g_activeProfileName;
bool g_overlayRunning = false;
CrosshairConfig g_currentConfig;

int ParseIntText(const wchar_t* text, int fallback)
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

std::wstring IntToWString(int value)
{
    wchar_t buffer[32] = {};
    swprintf(buffer, 32, L"%d", value);
    return buffer;
}

std::wstring GetExeDir()
{
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);

    std::wstring fullPath(path);
    const size_t split = fullPath.find_last_of(L"\\/");
    if (split == std::wstring::npos) {
        return L".";
    }

    return fullPath.substr(0, split);
}

std::wstring BuildProfilePath(const std::wstring& profileName)
{
    return g_configDir + L"\\" + profileName;
}

std::vector<std::wstring> EnumerateProfiles()
{
    std::vector<std::wstring> files;
    const std::wstring pattern = g_configDir + L"\\*.ini";

    WIN32_FIND_DATAW findData = {};
    HANDLE handle = FindFirstFileW(pattern.c_str(), &findData);
    if (handle == INVALID_HANDLE_VALUE) {
        return files;
    }

    do {
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            files.emplace_back(findData.cFileName);
        }
    } while (FindNextFileW(handle, &findData));

    FindClose(handle);
    std::sort(files.begin(), files.end());
    return files;
}

int ClampInt(int value, int minValue, int maxValue)
{
    return std::max(minValue, std::min(maxValue, value));
}

void NormalizeConfig(CrosshairConfig& config)
{
    config.windowSize = ClampInt(config.windowSize, 20, 800);
    config.lineWidth = ClampInt(config.lineWidth, 1, 20);
    config.crossHalf = ClampInt(config.crossHalf, 1, config.windowSize / 2);

    config.colorR = ClampInt(config.colorR, 0, 255);
    config.colorG = ClampInt(config.colorG, 0, 255);
    config.colorB = ClampInt(config.colorB, 0, 255);

    if (config.x < -1) config.x = -1;
    if (config.y < -1) config.y = -1;
}

CrosshairConfig DefaultConfig()
{
    CrosshairConfig config;
    NormalizeConfig(config);
    return config;
}

CrosshairConfig LoadConfigFromFile(const std::wstring& profilePath)
{
    CrosshairConfig config = DefaultConfig();

    config.x = GetPrivateProfileIntW(L"Crosshair", L"x", config.x, profilePath.c_str());
    config.y = GetPrivateProfileIntW(L"Crosshair", L"y", config.y, profilePath.c_str());
    config.windowSize = GetPrivateProfileIntW(L"Crosshair", L"window_size", config.windowSize, profilePath.c_str());
    config.crossHalf = GetPrivateProfileIntW(L"Crosshair", L"cross_half", config.crossHalf, profilePath.c_str());
    config.lineWidth = GetPrivateProfileIntW(L"Crosshair", L"line_width", config.lineWidth, profilePath.c_str());
    config.colorR = GetPrivateProfileIntW(L"Crosshair", L"color_r", config.colorR, profilePath.c_str());
    config.colorG = GetPrivateProfileIntW(L"Crosshair", L"color_g", config.colorG, profilePath.c_str());
    config.colorB = GetPrivateProfileIntW(L"Crosshair", L"color_b", config.colorB, profilePath.c_str());

    NormalizeConfig(config);
    return config;
}

bool SaveConfigToFile(const std::wstring& profilePath, CrosshairConfig config)
{
    NormalizeConfig(config);

    const BOOL okX = WritePrivateProfileStringW(L"Crosshair", L"x", IntToWString(config.x).c_str(), profilePath.c_str());
    const BOOL okY = WritePrivateProfileStringW(L"Crosshair", L"y", IntToWString(config.y).c_str(), profilePath.c_str());
    const BOOL okWindow = WritePrivateProfileStringW(L"Crosshair", L"window_size", IntToWString(config.windowSize).c_str(), profilePath.c_str());
    const BOOL okHalf = WritePrivateProfileStringW(L"Crosshair", L"cross_half", IntToWString(config.crossHalf).c_str(), profilePath.c_str());
    const BOOL okLine = WritePrivateProfileStringW(L"Crosshair", L"line_width", IntToWString(config.lineWidth).c_str(), profilePath.c_str());
    const BOOL okR = WritePrivateProfileStringW(L"Crosshair", L"color_r", IntToWString(config.colorR).c_str(), profilePath.c_str());
    const BOOL okG = WritePrivateProfileStringW(L"Crosshair", L"color_g", IntToWString(config.colorG).c_str(), profilePath.c_str());
    const BOOL okB = WritePrivateProfileStringW(L"Crosshair", L"color_b", IntToWString(config.colorB).c_str(), profilePath.c_str());

    return okX && okY && okWindow && okHalf && okLine && okR && okG && okB;
}

void SetEditValue(HWND edit, int value)
{
    SetWindowTextW(edit, IntToWString(value).c_str());
}

int ReadEditValue(HWND edit, int fallback)
{
    wchar_t text[64] = {};
    GetWindowTextW(edit, text, 64);
    return ParseIntText(text, fallback);
}

void FillUiFromConfig(const CrosshairConfig& config)
{
    SetEditValue(g_editX, config.x);
    SetEditValue(g_editY, config.y);
    SetEditValue(g_editWindowSize, config.windowSize);
    SetEditValue(g_editCrossHalf, config.crossHalf);
    SetEditValue(g_editLineWidth, config.lineWidth);
    SetEditValue(g_editR, config.colorR);
    SetEditValue(g_editG, config.colorG);
    SetEditValue(g_editB, config.colorB);
}

CrosshairConfig ReadConfigFromUi()
{
    CrosshairConfig config = g_currentConfig;

    config.x = ReadEditValue(g_editX, config.x);
    config.y = ReadEditValue(g_editY, config.y);
    config.windowSize = ReadEditValue(g_editWindowSize, config.windowSize);
    config.crossHalf = ReadEditValue(g_editCrossHalf, config.crossHalf);
    config.lineWidth = ReadEditValue(g_editLineWidth, config.lineWidth);
    config.colorR = ReadEditValue(g_editR, config.colorR);
    config.colorG = ReadEditValue(g_editG, config.colorG);
    config.colorB = ReadEditValue(g_editB, config.colorB);

    NormalizeConfig(config);
    return config;
}

void UpdateToggleButtonText()
{
    SetWindowTextW(g_toggleButton, g_overlayRunning ? L"关闭准星" : L"打开准星");
}

RECT ComputeOverlayRect(const CrosshairConfig& config)
{
    const int screenW = GetSystemMetrics(SM_CXSCREEN);
    const int screenH = GetSystemMetrics(SM_CYSCREEN);

    int centerX = config.x;
    int centerY = config.y;

    if (centerX < 0) centerX = screenW / 2;
    if (centerY < 0) centerY = screenH / 2;

    centerX = ClampInt(centerX, 0, screenW - 1);
    centerY = ClampInt(centerY, 0, screenH - 1);

    RECT rect = {};
    rect.left = centerX - config.windowSize / 2;
    rect.top = centerY - config.windowSize / 2;
    rect.right = rect.left + config.windowSize;
    rect.bottom = rect.top + config.windowSize;
    return rect;
}

void ApplyOverlayLayout()
{
    if (g_overlayWindow == nullptr) {
        return;
    }

    RECT rect = ComputeOverlayRect(g_currentConfig);
    MoveWindow(
        g_overlayWindow,
        rect.left,
        rect.top,
        rect.right - rect.left,
        rect.bottom - rect.top,
        TRUE);
    InvalidateRect(g_overlayWindow, nullptr, TRUE);
}

void StopOverlay()
{
    if (g_overlayWindow != nullptr) {
        DestroyWindow(g_overlayWindow);
        g_overlayWindow = nullptr;
    }

    g_overlayRunning = false;
    UpdateToggleButtonText();
}

bool StartOverlay()
{
    g_currentConfig = ReadConfigFromUi();

    RECT rect = ComputeOverlayRect(g_currentConfig);
    g_overlayWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        kOverlayWindowClass,
        L"",
        WS_POPUP,
        rect.left,
        rect.top,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        g_instance,
        nullptr);

    if (g_overlayWindow == nullptr) {
        MessageBoxW(g_mainWindow, L"创建准星窗口失败。", L"MyCross", MB_OK | MB_ICONERROR);
        return false;
    }

    SetLayeredWindowAttributes(g_overlayWindow, RGB(0, 0, 0), 0, LWA_COLORKEY);
    ShowWindow(g_overlayWindow, SW_SHOW);

    g_overlayRunning = true;
    UpdateToggleButtonText();
    return true;
}

void ToggleOverlay()
{
    if (g_overlayRunning) {
        StopOverlay();
    } else {
        StartOverlay();
    }
}

void ApplyConfigToOverlayIfRunning()
{
    if (!g_overlayRunning) {
        return;
    }

    g_currentConfig = ReadConfigFromUi();
    ApplyOverlayLayout();
}

std::wstring GenerateNextProfileName()
{
    const std::vector<std::wstring> profiles = EnumerateProfiles();

    for (int i = 1; i < 10000; ++i) {
        wchar_t name[64] = {};
        swprintf(name, 64, L"profile_%03d.ini", i);

        const bool exists = std::find(profiles.begin(), profiles.end(), name) != profiles.end();
        if (!exists) {
            return name;
        }
    }

    return L"profile_custom.ini";
}

bool SelectProfileInComboByName(const std::wstring& profileName)
{
    const int count = static_cast<int>(SendMessageW(g_profileCombo, CB_GETCOUNT, 0, 0));
    for (int i = 0; i < count; ++i) {
        wchar_t item[260] = {};
        SendMessageW(g_profileCombo, CB_GETLBTEXT, i, reinterpret_cast<LPARAM>(item));
        if (profileName == item) {
            SendMessageW(g_profileCombo, CB_SETCURSEL, i, 0);
            return true;
        }
    }

    return false;
}

std::wstring GetSelectedProfileName()
{
    wchar_t name[260] = {};
    const int selected = static_cast<int>(SendMessageW(g_profileCombo, CB_GETCURSEL, 0, 0));
    if (selected == CB_ERR) {
        return L"";
    }

    SendMessageW(g_profileCombo, CB_GETLBTEXT, selected, reinterpret_cast<LPARAM>(name));
    return name;
}

void LoadSelectedProfileToUi()
{
    const std::wstring profileName = GetSelectedProfileName();
    if (profileName.empty()) {
        return;
    }

    const std::wstring profilePath = BuildProfilePath(profileName);
    g_currentConfig = LoadConfigFromFile(profilePath);
    g_activeProfileName = profileName;
    FillUiFromConfig(g_currentConfig);
    ApplyConfigToOverlayIfRunning();
}

void RefreshProfileList(const std::wstring& preferred = L"")
{
    const std::vector<std::wstring> profiles = EnumerateProfiles();

    SendMessageW(g_profileCombo, CB_RESETCONTENT, 0, 0);
    for (const auto& profile : profiles) {
        SendMessageW(g_profileCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(profile.c_str()));
    }

    if (!preferred.empty() && SelectProfileInComboByName(preferred)) {
        LoadSelectedProfileToUi();
        return;
    }

    if (!g_activeProfileName.empty() && SelectProfileInComboByName(g_activeProfileName)) {
        LoadSelectedProfileToUi();
        return;
    }

    if (!profiles.empty()) {
        SendMessageW(g_profileCombo, CB_SETCURSEL, 0, 0);
        LoadSelectedProfileToUi();
    }
}

void EnsureConfigFolderAndDefaultProfile()
{
    CreateDirectoryW(g_configDir.c_str(), nullptr);

    std::vector<std::wstring> profiles = EnumerateProfiles();
    if (profiles.empty()) {
        const std::wstring defaultProfile = BuildProfilePath(L"default.ini");
        SaveConfigToFile(defaultProfile, DefaultConfig());
    }
}

void SaveCurrentProfile()
{
    if (g_activeProfileName.empty()) {
        MessageBoxW(g_mainWindow, L"请先选择一个配置文件。", L"MyCross", MB_OK | MB_ICONWARNING);
        return;
    }

    g_currentConfig = ReadConfigFromUi();
    const std::wstring path = BuildProfilePath(g_activeProfileName);

    if (!SaveConfigToFile(path, g_currentConfig)) {
        MessageBoxW(g_mainWindow, L"保存配置失败。", L"MyCross", MB_OK | MB_ICONERROR);
        return;
    }

    ApplyConfigToOverlayIfRunning();
    MessageBoxW(g_mainWindow, L"配置已保存。", L"MyCross", MB_OK | MB_ICONINFORMATION);
}

void CreateNewProfileFromCurrent()
{
    g_currentConfig = ReadConfigFromUi();

    const std::wstring profileName = GenerateNextProfileName();
    const std::wstring profilePath = BuildProfilePath(profileName);

    if (!SaveConfigToFile(profilePath, g_currentConfig)) {
        MessageBoxW(g_mainWindow, L"新建配置失败。", L"MyCross", MB_OK | MB_ICONERROR);
        return;
    }

    g_activeProfileName = profileName;
    RefreshProfileList(profileName);
    MessageBoxW(g_mainWindow, L"已基于当前参数创建新配置。", L"MyCross", MB_OK | MB_ICONINFORMATION);
}

HWND CreateLabel(HWND parent, const wchar_t* text, int x, int y, int w, int h)
{
    return CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, h, parent, nullptr, g_instance, nullptr);
}

HWND CreateEdit(HWND parent, int id, int x, int y, int w, int h)
{
    return CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        x,
        y,
        w,
        h,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        g_instance,
        nullptr);
}

void CreateMainControls(HWND hwnd)
{
    CreateLabel(hwnd, L"配置文件", 20, 20, 80, 24);

    g_profileCombo = CreateWindowW(
        L"COMBOBOX",
        L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        100,
        18,
        250,
        220,
        hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PROFILE_COMBO)),
        g_instance,
        nullptr);

    CreateWindowW(L"BUTTON", L"刷新", WS_CHILD | WS_VISIBLE, 365, 18, 60, 24, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BUTTON_REFRESH)), g_instance, nullptr);
    CreateWindowW(L"BUTTON", L"新建", WS_CHILD | WS_VISIBLE, 430, 18, 60, 24, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BUTTON_NEW)), g_instance, nullptr);
    CreateWindowW(L"BUTTON", L"保存", WS_CHILD | WS_VISIBLE, 365, 50, 125, 28, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BUTTON_SAVE)), g_instance, nullptr);

    CreateLabel(hwnd, L"坐标X (-1=居中)", 20, 95, 120, 24);
    CreateLabel(hwnd, L"坐标Y (-1=居中)", 20, 129, 120, 24);
    CreateLabel(hwnd, L"窗口尺寸", 20, 163, 120, 24);
    CreateLabel(hwnd, L"准星半径", 20, 197, 120, 24);
    CreateLabel(hwnd, L"线宽", 20, 231, 120, 24);
    CreateLabel(hwnd, L"颜色R", 270, 95, 60, 24);
    CreateLabel(hwnd, L"颜色G", 270, 129, 60, 24);
    CreateLabel(hwnd, L"颜色B", 270, 163, 60, 24);

    g_editX = CreateEdit(hwnd, IDC_EDIT_X, 145, 92, 100, 24);
    g_editY = CreateEdit(hwnd, IDC_EDIT_Y, 145, 126, 100, 24);
    g_editWindowSize = CreateEdit(hwnd, IDC_EDIT_WINDOW_SIZE, 145, 160, 100, 24);
    g_editCrossHalf = CreateEdit(hwnd, IDC_EDIT_CROSS_HALF, 145, 194, 100, 24);
    g_editLineWidth = CreateEdit(hwnd, IDC_EDIT_LINE_WIDTH, 145, 228, 100, 24);
    g_editR = CreateEdit(hwnd, IDC_EDIT_R, 335, 92, 80, 24);
    g_editG = CreateEdit(hwnd, IDC_EDIT_G, 335, 126, 80, 24);
    g_editB = CreateEdit(hwnd, IDC_EDIT_B, 335, 160, 80, 24);

    g_toggleButton = CreateWindowW(
        L"BUTTON",
        L"打开准星",
        WS_CHILD | WS_VISIBLE,
        20,
        285,
        470,
        45,
        hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BUTTON_TOGGLE)),
        g_instance,
        nullptr);

    CreateLabel(hwnd, L"热键关闭：Ctrl + Alt + Shift + F12", 20, 345, 300, 22);
}

LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps = {};
        HDC hdc = BeginPaint(hwnd, &ps);

        HPEN pen = CreatePen(PS_SOLID, g_currentConfig.lineWidth, RGB(g_currentConfig.colorR, g_currentConfig.colorG, g_currentConfig.colorB));
        HGDIOBJ oldPen = SelectObject(hdc, pen);

        const int cx = g_currentConfig.windowSize / 2;
        const int cy = g_currentConfig.windowSize / 2;

        MoveToEx(hdc, cx - g_currentConfig.crossHalf, cy, nullptr);
        LineTo(hdc, cx + g_currentConfig.crossHalf, cy);
        MoveToEx(hdc, cx, cy - g_currentConfig.crossHalf, nullptr);
        LineTo(hdc, cx, cy + g_currentConfig.crossHalf);

        SelectObject(hdc, oldPen);
        DeleteObject(pen);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        if (hwnd == g_overlayWindow) {
            g_overlayWindow = nullptr;
            g_overlayRunning = false;
            UpdateToggleButtonText();
        }
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        g_mainWindow = hwnd;
        CreateMainControls(hwnd);
        EnsureConfigFolderAndDefaultProfile();
        RefreshProfileList(L"default.ini");
        RegisterHotKey(hwnd, kHotkeyId, kQuitHotkeyModifiers, kQuitHotkeyVirtualKey);
        return 0;

    case WM_COMMAND: {
        const int id = LOWORD(wParam);
        const int notifyCode = HIWORD(wParam);

        if (id == IDC_PROFILE_COMBO && notifyCode == CBN_SELCHANGE) {
            LoadSelectedProfileToUi();
            return 0;
        }

        switch (id) {
        case IDC_BUTTON_REFRESH:
            RefreshProfileList();
            return 0;

        case IDC_BUTTON_NEW:
            CreateNewProfileFromCurrent();
            return 0;

        case IDC_BUTTON_SAVE:
            SaveCurrentProfile();
            return 0;

        case IDC_BUTTON_TOGGLE:
            ToggleOverlay();
            return 0;

        default:
            break;
        }

        return 0;
    }

    case WM_HOTKEY:
        if (wParam == kHotkeyId) {
            StopOverlay();
            return 0;
        }
        break;

    case WM_DESTROY:
        StopOverlay();
        UnregisterHotKey(hwnd, kHotkeyId);
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool RegisterMainWindowClass()
{
    WNDCLASSW wc = {};
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = g_instance;
    wc.lpszClassName = kMainWindowClass;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    return RegisterClassW(&wc) != 0;
}

bool RegisterOverlayWindowClass()
{
    WNDCLASSW wc = {};
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = g_instance;
    wc.lpszClassName = kOverlayWindowClass;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));

    return RegisterClassW(&wc) != 0;
}

void ApplyCommandLineInitialConfig()
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr) {
        return;
    }

    if (argc >= 3) {
        g_currentConfig.x = ParseIntText(argv[1], g_currentConfig.x);
        g_currentConfig.y = ParseIntText(argv[2], g_currentConfig.y);
    }

    for (int i = 1; i < argc; ++i) {
        if (wcsncmp(argv[i], L"--x=", 4) == 0) {
            g_currentConfig.x = ParseIntText(argv[i] + 4, g_currentConfig.x);
        } else if (wcsncmp(argv[i], L"--y=", 4) == 0) {
            g_currentConfig.y = ParseIntText(argv[i] + 4, g_currentConfig.y);
        }
    }

    NormalizeConfig(g_currentConfig);
    LocalFree(argv);
}
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCmd)
{
    g_instance = instance;
    g_exeDir = GetExeDir();
    g_configDir = g_exeDir + L"\\configs";
    g_currentConfig = DefaultConfig();
    ApplyCommandLineInitialConfig();

    if (!RegisterMainWindowClass() || !RegisterOverlayWindowClass()) {
        MessageBoxW(nullptr, L"窗口类注册失败。", L"MyCross", MB_OK | MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateWindowW(
        kMainWindowClass,
        L"MyCross 控制台",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        kMainWindowWidth,
        kMainWindowHeight,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (hwnd == nullptr) {
        MessageBoxW(nullptr, L"主窗口创建失败。", L"MyCross", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, showCmd == 0 ? SW_SHOW : showCmd);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
