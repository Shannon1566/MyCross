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

constexpr int kMainWindowWidth = 700;
constexpr int kMainWindowHeight = 560;

constexpr int kHotkeyId = 1;
constexpr UINT kQuitHotkeyModifiers = MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT;
constexpr UINT kQuitHotkeyVirtualKey = VK_F12;

constexpr int IDC_PROFILE_COMBO = 1001;
constexpr int IDC_BUTTON_REFRESH = 1002;
constexpr int IDC_BUTTON_NEW = 1003;
constexpr int IDC_BUTTON_SAVE = 1004;
constexpr int IDC_BUTTON_TOGGLE = 1005;
constexpr int IDC_COLOR_SWATCH = 1006;
constexpr int IDC_STATUS_LABEL = 1007;

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
HWND g_buttonRefresh = nullptr;
HWND g_buttonNew = nullptr;
HWND g_buttonSave = nullptr;
HWND g_statusLabel = nullptr;
HWND g_colorSwatch = nullptr;
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
HFONT g_fontRegular = nullptr;
HFONT g_fontTitle = nullptr;
HFONT g_fontSection = nullptr;
HBRUSH g_swatchBrush = nullptr;
HBRUSH g_mainBgBrush = nullptr;
HBRUSH g_cardBrush = nullptr;

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

void CreateUiFonts()
{
    if (g_fontRegular == nullptr) {
        g_fontRegular = CreateFontW(
            -18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    }

    if (g_fontTitle == nullptr) {
        g_fontTitle = CreateFontW(
            -30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    }

    if (g_fontSection == nullptr) {
        g_fontSection = CreateFontW(
            -20, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    }

    if (g_mainBgBrush == nullptr) {
        g_mainBgBrush = CreateSolidBrush(RGB(241, 244, 249));
    }

    if (g_cardBrush == nullptr) {
        g_cardBrush = CreateSolidBrush(RGB(255, 255, 255));
    }
}

void DestroyUiResources()
{
    if (g_swatchBrush != nullptr) {
        DeleteObject(g_swatchBrush);
        g_swatchBrush = nullptr;
    }
    if (g_fontRegular != nullptr) {
        DeleteObject(g_fontRegular);
        g_fontRegular = nullptr;
    }
    if (g_fontTitle != nullptr) {
        DeleteObject(g_fontTitle);
        g_fontTitle = nullptr;
    }
    if (g_fontSection != nullptr) {
        DeleteObject(g_fontSection);
        g_fontSection = nullptr;
    }
    if (g_mainBgBrush != nullptr) {
        DeleteObject(g_mainBgBrush);
        g_mainBgBrush = nullptr;
    }
    if (g_cardBrush != nullptr) {
        DeleteObject(g_cardBrush);
        g_cardBrush = nullptr;
    }
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

void SetControlFont(HWND control, HFONT font)
{
    if (control != nullptr && font != nullptr) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
}

int ReadEditValue(HWND edit, int fallback)
{
    wchar_t text[64] = {};
    GetWindowTextW(edit, text, 64);
    return ParseIntText(text, fallback);
}

void UpdateColorSwatch(const CrosshairConfig& config)
{
    if (g_swatchBrush != nullptr) {
        DeleteObject(g_swatchBrush);
        g_swatchBrush = nullptr;
    }

    g_swatchBrush = CreateSolidBrush(RGB(config.colorR, config.colorG, config.colorB));
    if (g_colorSwatch != nullptr) {
        InvalidateRect(g_colorSwatch, nullptr, TRUE);
    }
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
    UpdateColorSwatch(config);
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
    SetWindowTextW(g_toggleButton, g_overlayRunning ? L"停止准星叠加" : L"启动准星叠加");
    if (g_statusLabel != nullptr) {
        SetWindowTextW(g_statusLabel, g_overlayRunning ? L"状态: 运行中" : L"状态: 已停止");
        InvalidateRect(g_statusLabel, nullptr, TRUE);
        UpdateWindow(g_statusLabel);
    }
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
    g_currentConfig = ReadConfigFromUi();
    UpdateColorSwatch(g_currentConfig);

    if (!g_overlayRunning) {
        return;
    }

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
    HWND label = CreateWindowW(
        L"STATIC",
        text,
        WS_CHILD | WS_VISIBLE,
        x,
        y,
        w,
        h,
        parent,
        nullptr,
        g_instance,
        nullptr);
    SetControlFont(label, g_fontRegular);
    return label;
}

HWND CreateEdit(HWND parent, int id, int x, int y, int w, int h)
{
    HWND edit = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_CENTER,
        x,
        y,
        w,
        h,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        g_instance,
        nullptr);
    SetControlFont(edit, g_fontRegular);
    return edit;
}

HWND CreateButton(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h)
{
    HWND button = CreateWindowW(
        L"BUTTON",
        text,
        WS_CHILD | WS_VISIBLE | BS_FLAT,
        x,
        y,
        w,
        h,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        g_instance,
        nullptr);
    SetControlFont(button, g_fontRegular);
    return button;
}

HWND CreateSectionTitle(HWND parent, const wchar_t* text, int x, int y, int w, int h)
{
    HWND label = CreateWindowW(
        L"STATIC",
        text,
        WS_CHILD | WS_VISIBLE,
        x,
        y,
        w,
        h,
        parent,
        nullptr,
        g_instance,
        nullptr);
    SetControlFont(label, g_fontSection);
    return label;
}

void CreateMainControls(HWND hwnd)
{
    CreateUiFonts();

    HWND title = CreateWindowW(L"STATIC", L"MyCross 控制台", WS_CHILD | WS_VISIBLE,
        28, 18, 340, 42, hwnd, nullptr, g_instance, nullptr);
    SetControlFont(title, g_fontTitle);
    CreateLabel(hwnd, L"专业级准星控制面板", 30, 62, 320, 24);

    CreateSectionTitle(hwnd, L"配置管理", 36, 98, 140, 28);
    CreateLabel(hwnd, L"当前配置", 38, 136, 90, 24);
    g_profileCombo = CreateWindowW(
        L"COMBOBOX",
        L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        124,
        134,
        280,
        240,
        hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PROFILE_COMBO)),
        g_instance,
        nullptr);
    SetControlFont(g_profileCombo, g_fontRegular);

    g_buttonRefresh = CreateButton(hwnd, L"刷新", IDC_BUTTON_REFRESH, 420, 134, 80, 32);
    g_buttonNew = CreateButton(hwnd, L"新建配置", IDC_BUTTON_NEW, 508, 134, 110, 32);
    g_buttonSave = CreateButton(hwnd, L"保存当前配置", IDC_BUTTON_SAVE, 420, 176, 198, 32);

    CreateSectionTitle(hwnd, L"准星参数", 36, 232, 140, 28);
    CreateLabel(hwnd, L"X (-1=居中)", 38, 272, 120, 24);
    CreateLabel(hwnd, L"Y (-1=居中)", 38, 314, 120, 24);
    CreateLabel(hwnd, L"窗口尺寸", 38, 356, 120, 24);
    CreateLabel(hwnd, L"准星半径", 38, 398, 120, 24);
    CreateLabel(hwnd, L"线宽", 38, 440, 120, 24);

    g_editX = CreateEdit(hwnd, IDC_EDIT_X, 136, 270, 120, 30);
    g_editY = CreateEdit(hwnd, IDC_EDIT_Y, 136, 312, 120, 30);
    g_editWindowSize = CreateEdit(hwnd, IDC_EDIT_WINDOW_SIZE, 136, 354, 120, 30);
    g_editCrossHalf = CreateEdit(hwnd, IDC_EDIT_CROSS_HALF, 136, 396, 120, 30);
    g_editLineWidth = CreateEdit(hwnd, IDC_EDIT_LINE_WIDTH, 136, 438, 120, 30);

    CreateLabel(hwnd, L"颜色 R", 308, 272, 70, 24);
    CreateLabel(hwnd, L"颜色 G", 308, 314, 70, 24);
    CreateLabel(hwnd, L"颜色 B", 308, 356, 70, 24);

    g_editR = CreateEdit(hwnd, IDC_EDIT_R, 380, 270, 100, 30);
    g_editG = CreateEdit(hwnd, IDC_EDIT_G, 380, 312, 100, 30);
    g_editB = CreateEdit(hwnd, IDC_EDIT_B, 380, 354, 100, 30);

    CreateLabel(hwnd, L"颜色预览", 514, 272, 100, 24);
    g_colorSwatch = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE,
        514,
        304,
        126,
        96,
        hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_COLOR_SWATCH)),
        g_instance,
        nullptr);
    SetControlFont(g_colorSwatch, g_fontRegular);

    CreateSectionTitle(hwnd, L"运行控制", 308, 408, 140, 28);
    g_toggleButton = CreateButton(hwnd, L"启动准星叠加", IDC_BUTTON_TOGGLE, 308, 440, 240, 44);
    g_statusLabel = CreateLabel(hwnd, L"状态: 已停止", 546, 444, 130, 24);
    CreateLabel(hwnd, L"全局热键: Ctrl + Alt + Shift + F12", 308, 492, 320, 24);
}

bool IsConfigEditId(int id)
{
    return id == IDC_EDIT_X || id == IDC_EDIT_Y || id == IDC_EDIT_WINDOW_SIZE ||
        id == IDC_EDIT_CROSS_HALF || id == IDC_EDIT_LINE_WIDTH ||
        id == IDC_EDIT_R || id == IDC_EDIT_G || id == IDC_EDIT_B;
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

        if (IsConfigEditId(id) && notifyCode == EN_CHANGE) {
            ApplyConfigToOverlayIfRunning();
            return 0;
        }

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

    case WM_CTLCOLORSTATIC: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        HWND ctrl = reinterpret_cast<HWND>(lParam);

        if (ctrl == g_colorSwatch && g_swatchBrush != nullptr) {
            SetBkMode(hdc, OPAQUE);
            return reinterpret_cast<INT_PTR>(g_swatchBrush);
        }

        SetTextColor(hdc, RGB(28, 33, 40));
        SetBkMode(hdc, TRANSPARENT);
        return reinterpret_cast<INT_PTR>(g_mainBgBrush != nullptr ? g_mainBgBrush : GetSysColorBrush(COLOR_WINDOW));
    }

    case WM_PAINT: {
        PAINTSTRUCT ps = {};
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc = {};
        GetClientRect(hwnd, &rc);

        FillRect(hdc, &rc, g_mainBgBrush != nullptr ? g_mainBgBrush : GetSysColorBrush(COLOR_WINDOW));

        RECT cardTop = { 24, 88, 676, 220 };
        RECT cardBody = { 24, 222, 676, 478 };
        RECT cardAction = { 292, 398, 676, 532 };

        FillRect(hdc, &cardTop, g_cardBrush != nullptr ? g_cardBrush : GetSysColorBrush(COLOR_WINDOW));
        FillRect(hdc, &cardBody, g_cardBrush != nullptr ? g_cardBrush : GetSysColorBrush(COLOR_WINDOW));
        FillRect(hdc, &cardAction, g_cardBrush != nullptr ? g_cardBrush : GetSysColorBrush(COLOR_WINDOW));

        HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(220, 226, 236));
        HGDIOBJ oldPen = SelectObject(hdc, borderPen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, cardTop.left, cardTop.top, cardTop.right, cardTop.bottom);
        Rectangle(hdc, cardBody.left, cardBody.top, cardBody.right, cardBody.bottom);
        Rectangle(hdc, cardAction.left, cardAction.top, cardAction.right, cardAction.bottom);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(borderPen);

        EndPaint(hwnd, &ps);
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
        DestroyUiResources();
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
