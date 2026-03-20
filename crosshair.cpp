#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {
constexpr wchar_t kOverlayClassName[] = L"MyCrossOverlayWindow";
constexpr wchar_t kControlClassName[] = L"MyCrossControlWindow";

constexpr UINT WM_APP_APPLY = WM_APP + 1;
constexpr int kHotkeyId = 1;
constexpr UINT kQuitHotkeyModifiers = MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT;
constexpr UINT kQuitHotkeyVirtualKey = VK_F12;

constexpr int kServerPort = 5188;

struct CrosshairConfig {
    int x = -1;
    int y = -1;
    int windowSize = 40;
    int crossHalf = 10;
    int lineWidth = 2;
    int colorR = 0;
    int colorG = 255;
    int colorB = 0;
};

std::mutex g_stateMutex;
CrosshairConfig g_config;
bool g_overlayRunning = false;
std::wstring g_activeProfile = L"default.ini";

std::wstring g_exeDir;
std::wstring g_configDir;
std::wstring g_webDir;

std::atomic<bool> g_exitRequested{false};

HINSTANCE g_instance = nullptr;
HWND g_controlWindow = nullptr;
HWND g_overlayWindow = nullptr;
DWORD g_overlayThreadId = 0;
std::thread g_overlayThread;
SOCKET g_listenSocket = INVALID_SOCKET;
PROCESS_INFORMATION g_webAppProcess = {};
DWORD g_lastAppLaunchError = 0;
std::atomic<DWORD> g_lastUiPingTick{0};
bool g_appWindowMode = false;
constexpr DWORD kUiHeartbeatTimeoutMs = 15000;

int ClampInt(int value, int minV, int maxV)
{
    return std::max(minV, std::min(maxV, value));
}

void NormalizeConfig(CrosshairConfig& c)
{
    c.windowSize = ClampInt(c.windowSize, 20, 800);
    c.lineWidth = ClampInt(c.lineWidth, 1, 20);
    c.crossHalf = ClampInt(c.crossHalf, 1, std::max(1, c.windowSize / 2));
    c.colorR = ClampInt(c.colorR, 0, 255);
    c.colorG = ClampInt(c.colorG, 0, 255);
    c.colorB = ClampInt(c.colorB, 0, 255);
    if (c.x < -1) c.x = -1;
    if (c.y < -1) c.y = -1;
}

int ParseInt(const std::string& text, int fallback)
{
    if (text.empty()) {
        return fallback;
    }

    char* end = nullptr;
    const long value = strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0') {
        return fallback;
    }
    return static_cast<int>(value);
}

std::wstring GetExeDir()
{
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring full(path);
    const size_t pos = full.find_last_of(L"\\/");
    if (pos == std::wstring::npos) {
        return L".";
    }
    return full.substr(0, pos);
}

std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty()) return L"";
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 0) return L"";
    std::wstring out(static_cast<size_t>(n - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), n);
    return out;
}

std::string WideToUtf8(const std::wstring& s)
{
    if (s.empty()) return "";
    const int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return "";
    std::string out(static_cast<size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring BuildProfilePath(const std::wstring& name)
{
    return g_configDir + L"\\" + name;
}

std::wstring Trim(const std::wstring& text)
{
    size_t left = 0;
    while (left < text.size() && iswspace(text[left])) ++left;
    size_t right = text.size();
    while (right > left && iswspace(text[right - 1])) --right;
    return text.substr(left, right - left);
}

std::wstring NormalizeProfileName(std::wstring name)
{
    name = Trim(name);
    for (auto& ch : name) {
        if (ch == L'\\' || ch == L'/' || ch == L':' || ch == L'*' || ch == L'?' ||
            ch == L'"' || ch == L'<' || ch == L'>' || ch == L'|') {
            ch = L'_';
        }
    }

    if (name.empty()) {
        return L"";
    }

    const std::wstring suffix = L".ini";
    if (name.size() < suffix.size() || _wcsicmp(name.c_str() + (name.size() - suffix.size()), suffix.c_str()) != 0) {
        name += suffix;
    }

    return name;
}

std::vector<std::wstring> EnumerateProfiles()
{
    std::vector<std::wstring> profiles;
    const std::wstring pattern = g_configDir + L"\\*.ini";

    WIN32_FIND_DATAW data = {};
    HANDLE h = FindFirstFileW(pattern.c_str(), &data);
    if (h == INVALID_HANDLE_VALUE) {
        return profiles;
    }

    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            profiles.emplace_back(data.cFileName);
        }
    } while (FindNextFileW(h, &data));

    FindClose(h);
    std::sort(profiles.begin(), profiles.end());
    return profiles;
}

CrosshairConfig LoadConfigFromFile(const std::wstring& path)
{
    CrosshairConfig c;
    c.x = GetPrivateProfileIntW(L"Crosshair", L"x", c.x, path.c_str());
    c.y = GetPrivateProfileIntW(L"Crosshair", L"y", c.y, path.c_str());
    c.windowSize = GetPrivateProfileIntW(L"Crosshair", L"window_size", c.windowSize, path.c_str());
    c.crossHalf = GetPrivateProfileIntW(L"Crosshair", L"cross_half", c.crossHalf, path.c_str());
    c.lineWidth = GetPrivateProfileIntW(L"Crosshair", L"line_width", c.lineWidth, path.c_str());
    c.colorR = GetPrivateProfileIntW(L"Crosshair", L"color_r", c.colorR, path.c_str());
    c.colorG = GetPrivateProfileIntW(L"Crosshair", L"color_g", c.colorG, path.c_str());
    c.colorB = GetPrivateProfileIntW(L"Crosshair", L"color_b", c.colorB, path.c_str());
    NormalizeConfig(c);
    return c;
}

std::wstring IntToWString(int v)
{
    wchar_t buf[32] = {};
    swprintf(buf, 32, L"%d", v);
    return buf;
}

bool SaveConfigToFile(const std::wstring& path, CrosshairConfig c)
{
    NormalizeConfig(c);
    const BOOL ok1 = WritePrivateProfileStringW(L"Crosshair", L"x", IntToWString(c.x).c_str(), path.c_str());
    const BOOL ok2 = WritePrivateProfileStringW(L"Crosshair", L"y", IntToWString(c.y).c_str(), path.c_str());
    const BOOL ok3 = WritePrivateProfileStringW(L"Crosshair", L"window_size", IntToWString(c.windowSize).c_str(), path.c_str());
    const BOOL ok4 = WritePrivateProfileStringW(L"Crosshair", L"cross_half", IntToWString(c.crossHalf).c_str(), path.c_str());
    const BOOL ok5 = WritePrivateProfileStringW(L"Crosshair", L"line_width", IntToWString(c.lineWidth).c_str(), path.c_str());
    const BOOL ok6 = WritePrivateProfileStringW(L"Crosshair", L"color_r", IntToWString(c.colorR).c_str(), path.c_str());
    const BOOL ok7 = WritePrivateProfileStringW(L"Crosshair", L"color_g", IntToWString(c.colorG).c_str(), path.c_str());
    const BOOL ok8 = WritePrivateProfileStringW(L"Crosshair", L"color_b", IntToWString(c.colorB).c_str(), path.c_str());
    return ok1 && ok2 && ok3 && ok4 && ok5 && ok6 && ok7 && ok8;
}

void EnsureConfigDirectory()
{
    CreateDirectoryW(g_configDir.c_str(), nullptr);
    const auto profiles = EnumerateProfiles();
    if (profiles.empty()) {
        SaveConfigToFile(BuildProfilePath(L"default.ini"), CrosshairConfig{});
    }
}

std::string JsonEscape(const std::string& input)
{
    std::string out;
    out.reserve(input.size() + 8);
    for (char ch : input) {
        switch (ch) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += ch; break;
        }
    }
    return out;
}

RECT ComputeOverlayRect(const CrosshairConfig& c)
{
    const int screenW = GetSystemMetrics(SM_CXSCREEN);
    const int screenH = GetSystemMetrics(SM_CYSCREEN);

    int centerX = c.x;
    int centerY = c.y;
    if (centerX < 0) centerX = screenW / 2;
    if (centerY < 0) centerY = screenH / 2;

    centerX = ClampInt(centerX, 0, screenW - 1);
    centerY = ClampInt(centerY, 0, screenH - 1);

    RECT r = {};
    r.left = centerX - c.windowSize / 2;
    r.top = centerY - c.windowSize / 2;
    r.right = r.left + c.windowSize;
    r.bottom = r.top + c.windowSize;
    return r;
}

void ApplyOverlayFromState()
{
    CrosshairConfig c;
    bool running = false;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        c = g_config;
        running = g_overlayRunning;
    }

    if (!running) {
        if (g_overlayWindow != nullptr) {
            DestroyWindow(g_overlayWindow);
            g_overlayWindow = nullptr;
        }
        return;
    }

    const RECT r = ComputeOverlayRect(c);
    if (g_overlayWindow == nullptr) {
        g_overlayWindow = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
            kOverlayClassName,
            L"",
            WS_POPUP,
            r.left,
            r.top,
            r.right - r.left,
            r.bottom - r.top,
            nullptr,
            nullptr,
            g_instance,
            nullptr);
        if (g_overlayWindow == nullptr) {
            return;
        }
        SetLayeredWindowAttributes(g_overlayWindow, RGB(0, 0, 0), 0, LWA_COLORKEY);
        ShowWindow(g_overlayWindow, SW_SHOW);
    } else {
        MoveWindow(g_overlayWindow, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
    }

    InvalidateRect(g_overlayWindow, nullptr, TRUE);
}

LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_PAINT: {
        CrosshairConfig c;
        {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            c = g_config;
        }

        PAINTSTRUCT ps = {};
        HDC hdc = BeginPaint(hwnd, &ps);

        HPEN pen = CreatePen(PS_SOLID, c.lineWidth, RGB(c.colorR, c.colorG, c.colorB));
        HGDIOBJ oldPen = SelectObject(hdc, pen);

        const int cx = c.windowSize / 2;
        const int cy = c.windowSize / 2;

        MoveToEx(hdc, cx - c.crossHalf, cy, nullptr);
        LineTo(hdc, cx + c.crossHalf, cy);
        MoveToEx(hdc, cx, cy - c.crossHalf, nullptr);
        LineTo(hdc, cx, cy + c.crossHalf);

        SelectObject(hdc, oldPen);
        DeleteObject(pen);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        if (hwnd == g_overlayWindow) {
            g_overlayWindow = nullptr;
        }
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK ControlWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_APP_APPLY:
        ApplyOverlayFromState();
        return 0;

    case WM_HOTKEY:
        if (wParam == kHotkeyId) {
            {
                std::lock_guard<std::mutex> lock(g_stateMutex);
                g_overlayRunning = false;
            }
            ApplyOverlayFromState();
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

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

DWORD WINAPI OverlayThreadProc(LPVOID)
{
    WNDCLASSW overlayClass = {};
    overlayClass.lpfnWndProc = OverlayWndProc;
    overlayClass.hInstance = g_instance;
    overlayClass.lpszClassName = kOverlayClassName;
    overlayClass.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    RegisterClassW(&overlayClass);

    WNDCLASSW controlClass = {};
    controlClass.lpfnWndProc = ControlWndProc;
    controlClass.hInstance = g_instance;
    controlClass.lpszClassName = kControlClassName;
    RegisterClassW(&controlClass);

    g_controlWindow = CreateWindowW(
        kControlClassName,
        L"",
        WS_OVERLAPPED,
        0,
        0,
        0,
        0,
        nullptr,
        nullptr,
        g_instance,
        nullptr);

    RegisterHotKey(g_controlWindow, kHotkeyId, kQuitHotkeyModifiers, kQuitHotkeyVirtualKey);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_overlayWindow != nullptr) {
        DestroyWindow(g_overlayWindow);
        g_overlayWindow = nullptr;
    }

    if (g_controlWindow != nullptr) {
        DestroyWindow(g_controlWindow);
        g_controlWindow = nullptr;
    }

    return 0;
}

void NotifyOverlayApply()
{
    HWND hwnd = g_controlWindow;
    if (hwnd != nullptr) {
        PostMessageW(hwnd, WM_APP_APPLY, 0, 0);
    }
}

std::string ReadFileUtf8(const std::wstring& path)
{
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) return "";
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string UrlDecode(const std::string& text)
{
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '%' && i + 2 < text.size()) {
            const std::string hex = text.substr(i + 1, 2);
            const int v = strtol(hex.c_str(), nullptr, 16);
            out.push_back(static_cast<char>(v));
            i += 2;
        } else if (text[i] == '+') {
            out.push_back(' ');
        } else {
            out.push_back(text[i]);
        }
    }
    return out;
}

std::map<std::string, std::string> ParseForm(const std::string& body)
{
    std::map<std::string, std::string> out;
    size_t start = 0;
    while (start <= body.size()) {
        const size_t amp = body.find('&', start);
        const std::string part = body.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
        const size_t eq = part.find('=');
        if (eq != std::string::npos) {
            const std::string key = UrlDecode(part.substr(0, eq));
            const std::string val = UrlDecode(part.substr(eq + 1));
            out[key] = val;
        } else if (!part.empty()) {
            out[UrlDecode(part)] = "";
        }

        if (amp == std::string::npos) break;
        start = amp + 1;
    }
    return out;
}

bool ReadHttpRequest(SOCKET client, std::string& request)
{
    char buffer[4096];
    request.clear();

    size_t contentLength = 0;
    bool headerParsed = false;
    size_t headerEndPos = std::string::npos;

    while (true) {
        const int n = recv(client, buffer, sizeof(buffer), 0);
        if (n <= 0) {
            return false;
        }
        request.append(buffer, buffer + n);

        if (!headerParsed) {
            headerEndPos = request.find("\r\n\r\n");
            if (headerEndPos != std::string::npos) {
                headerParsed = true;
                const std::string headers = request.substr(0, headerEndPos + 4);
                const std::string key = "Content-Length:";
                size_t pos = headers.find(key);
                if (pos != std::string::npos) {
                    pos += key.size();
                    while (pos < headers.size() && (headers[pos] == ' ' || headers[pos] == '\t')) ++pos;
                    size_t end = pos;
                    while (end < headers.size() && headers[end] >= '0' && headers[end] <= '9') ++end;
                    contentLength = static_cast<size_t>(strtoul(headers.substr(pos, end - pos).c_str(), nullptr, 10));
                }
            }
        }

        if (headerParsed) {
            const size_t bodyOffset = headerEndPos + 4;
            const size_t haveBody = request.size() - bodyOffset;
            if (haveBody >= contentLength) {
                return true;
            }
        }
    }
}

void SendHttp(SOCKET client, int code, const char* status, const char* contentType, const std::string& body)
{
    std::ostringstream out;
    out << "HTTP/1.1 " << code << ' ' << status << "\r\n";
    out << "Content-Type: " << contentType << "\r\n";
    out << "Content-Length: " << body.size() << "\r\n";
    out << "Connection: close\r\n";
    out << "Cache-Control: no-cache\r\n\r\n";
    out << body;

    const std::string response = out.str();
    send(client, response.c_str(), static_cast<int>(response.size()), 0);
}

std::string BuildStateJson()
{
    CrosshairConfig c;
    bool running;
    std::wstring active;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        c = g_config;
        running = g_overlayRunning;
        active = g_activeProfile;
    }

    const auto profiles = EnumerateProfiles();
    std::ostringstream json;
    json << "{";
    json << "\"running\":" << (running ? "true" : "false") << ",";
    json << "\"active_profile\":\"" << JsonEscape(WideToUtf8(active)) << "\",";
    json << "\"hotkey\":\"Ctrl+Alt+Shift+F12\",";
    json << "\"config\":{";
    json << "\"x\":" << c.x << ",";
    json << "\"y\":" << c.y << ",";
    json << "\"window_size\":" << c.windowSize << ",";
    json << "\"cross_half\":" << c.crossHalf << ",";
    json << "\"line_width\":" << c.lineWidth << ",";
    json << "\"color_r\":" << c.colorR << ",";
    json << "\"color_g\":" << c.colorG << ",";
    json << "\"color_b\":" << c.colorB << "},";
    json << "\"profiles\":[";
    for (size_t i = 0; i < profiles.size(); ++i) {
        if (i) json << ',';
        json << "\"" << JsonEscape(WideToUtf8(profiles[i])) << "\"";
    }
    json << "]}";
    return json.str();
}

CrosshairConfig ReadConfigFromForm(const std::map<std::string, std::string>& form, const CrosshairConfig& fallback)
{
    CrosshairConfig c = fallback;
    auto get = [&](const char* key, int oldV) -> int {
        const auto it = form.find(key);
        if (it == form.end()) return oldV;
        return ParseInt(it->second, oldV);
    };

    c.x = get("x", c.x);
    c.y = get("y", c.y);
    c.windowSize = get("window_size", c.windowSize);
    c.crossHalf = get("cross_half", c.crossHalf);
    c.lineWidth = get("line_width", c.lineWidth);
    c.colorR = get("color_r", c.colorR);
    c.colorG = get("color_g", c.colorG);
    c.colorB = get("color_b", c.colorB);
    NormalizeConfig(c);
    return c;
}

void ApplyCommandLineArgs()
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return;

    CrosshairConfig c;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        c = g_config;
    }

    if (argc >= 3) {
        c.x = _wtoi(argv[1]);
        c.y = _wtoi(argv[2]);
    }

    for (int i = 1; i < argc; ++i) {
        if (wcsncmp(argv[i], L"--x=", 4) == 0) c.x = _wtoi(argv[i] + 4);
        else if (wcsncmp(argv[i], L"--y=", 4) == 0) c.y = _wtoi(argv[i] + 4);
    }

    NormalizeConfig(c);
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        g_config = c;
    }

    LocalFree(argv);
}

bool HandleApi(SOCKET client, const std::string& method, const std::string& path, const std::string& body)
{
    g_lastUiPingTick = GetTickCount();

    if (method == "GET" && path == "/api/state") {
        SendHttp(client, 200, "OK", "application/json; charset=utf-8", BuildStateJson());
        return true;
    }

    if (method == "POST" && path == "/api/ping") {
        SendHttp(client, 200, "OK", "text/plain; charset=utf-8", "pong");
        return true;
    }

    if (method == "POST" && path == "/api/toggle") {
        const auto form = ParseForm(body);
        const bool running = form.find("running") != form.end() && form.at("running") == "1";
        {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            g_overlayRunning = running;
        }
        NotifyOverlayApply();
        SendHttp(client, 200, "OK", "application/json; charset=utf-8", BuildStateJson());
        return true;
    }

    if (method == "POST" && path == "/api/apply") {
        const auto form = ParseForm(body);
        {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            g_config = ReadConfigFromForm(form, g_config);
        }
        NotifyOverlayApply();
        SendHttp(client, 200, "OK", "application/json; charset=utf-8", BuildStateJson());
        return true;
    }

    if (method == "POST" && path == "/api/profile/load") {
        const auto form = ParseForm(body);
        const auto it = form.find("name");
        if (it == form.end()) {
            SendHttp(client, 400, "Bad Request", "text/plain; charset=utf-8", "missing name");
            return true;
        }

        const std::wstring name = NormalizeProfileName(Utf8ToWide(it->second));
        if (name.empty()) {
            SendHttp(client, 400, "Bad Request", "text/plain; charset=utf-8", "invalid name");
            return true;
        }

        const std::wstring pathW = BuildProfilePath(name);
        if (GetFileAttributesW(pathW.c_str()) == INVALID_FILE_ATTRIBUTES) {
            SendHttp(client, 404, "Not Found", "text/plain; charset=utf-8", "profile not found");
            return true;
        }

        CrosshairConfig loaded = LoadConfigFromFile(pathW);
        {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            g_config = loaded;
            g_activeProfile = name;
        }
        NotifyOverlayApply();
        SendHttp(client, 200, "OK", "application/json; charset=utf-8", BuildStateJson());
        return true;
    }

    if (method == "POST" && path == "/api/profile/save") {
        const auto form = ParseForm(body);
        std::wstring name;
        {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            name = g_activeProfile;
        }

        const auto itName = form.find("name");
        if (itName != form.end()) {
            const std::wstring named = NormalizeProfileName(Utf8ToWide(itName->second));
            if (!named.empty()) name = named;
        }

        if (name.empty()) {
            SendHttp(client, 400, "Bad Request", "text/plain; charset=utf-8", "invalid profile name");
            return true;
        }

        CrosshairConfig cfg;
        {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            g_config = ReadConfigFromForm(form, g_config);
            cfg = g_config;
            g_activeProfile = name;
        }

        if (!SaveConfigToFile(BuildProfilePath(name), cfg)) {
            SendHttp(client, 500, "Internal Server Error", "text/plain; charset=utf-8", "save failed");
            return true;
        }

        NotifyOverlayApply();
        SendHttp(client, 200, "OK", "application/json; charset=utf-8", BuildStateJson());
        return true;
    }

    if (method == "POST" && path == "/api/profile/new") {
        const auto form = ParseForm(body);
        const auto itName = form.find("name");
        if (itName == form.end()) {
            SendHttp(client, 400, "Bad Request", "text/plain; charset=utf-8", "missing name");
            return true;
        }

        std::wstring name = NormalizeProfileName(Utf8ToWide(itName->second));
        if (name.empty()) {
            SendHttp(client, 400, "Bad Request", "text/plain; charset=utf-8", "invalid profile name");
            return true;
        }

        const std::wstring pathW = BuildProfilePath(name);
        if (GetFileAttributesW(pathW.c_str()) != INVALID_FILE_ATTRIBUTES) {
            SendHttp(client, 409, "Conflict", "text/plain; charset=utf-8", "profile exists");
            return true;
        }

        CrosshairConfig cfg;
        {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            g_config = ReadConfigFromForm(form, g_config);
            cfg = g_config;
            g_activeProfile = name;
        }

        if (!SaveConfigToFile(pathW, cfg)) {
            SendHttp(client, 500, "Internal Server Error", "text/plain; charset=utf-8", "create failed");
            return true;
        }

        NotifyOverlayApply();
        SendHttp(client, 200, "OK", "application/json; charset=utf-8", BuildStateJson());
        return true;
    }

    if (method == "POST" && path == "/api/profile/rename") {
        const auto form = ParseForm(body);
        const auto itOld = form.find("old_name");
        const auto itNew = form.find("new_name");
        if (itOld == form.end() || itNew == form.end()) {
            SendHttp(client, 400, "Bad Request", "text/plain; charset=utf-8", "missing names");
            return true;
        }

        std::wstring oldName = NormalizeProfileName(Utf8ToWide(itOld->second));
        std::wstring newName = NormalizeProfileName(Utf8ToWide(itNew->second));
        if (oldName.empty() || newName.empty()) {
            SendHttp(client, 400, "Bad Request", "text/plain; charset=utf-8", "invalid name");
            return true;
        }

        const std::wstring oldPath = BuildProfilePath(oldName);
        const std::wstring newPath = BuildProfilePath(newName);
        if (GetFileAttributesW(newPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            SendHttp(client, 409, "Conflict", "text/plain; charset=utf-8", "target exists");
            return true;
        }

        if (!MoveFileW(oldPath.c_str(), newPath.c_str())) {
            SendHttp(client, 500, "Internal Server Error", "text/plain; charset=utf-8", "rename failed");
            return true;
        }

        {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            if (_wcsicmp(g_activeProfile.c_str(), oldName.c_str()) == 0) {
                g_activeProfile = newName;
            }
        }

        SendHttp(client, 200, "OK", "application/json; charset=utf-8", BuildStateJson());
        return true;
    }

    if (method == "POST" && path == "/api/quit") {
        g_exitRequested = true;
        if (g_listenSocket != INVALID_SOCKET) {
            closesocket(g_listenSocket);
            g_listenSocket = INVALID_SOCKET;
        }
        SendHttp(client, 200, "OK", "text/plain; charset=utf-8", "bye");
        return true;
    }

    return false;
}

void HandleClient(SOCKET client)
{
    std::string raw;
    if (!ReadHttpRequest(client, raw)) {
        closesocket(client);
        return;
    }

    const size_t headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        SendHttp(client, 400, "Bad Request", "text/plain; charset=utf-8", "bad request");
        closesocket(client);
        return;
    }

    const std::string headers = raw.substr(0, headerEnd);
    const std::string body = raw.substr(headerEnd + 4);

    std::istringstream hs(headers);
    std::string requestLine;
    std::getline(hs, requestLine);
    if (!requestLine.empty() && requestLine.back() == '\r') {
        requestLine.pop_back();
    }

    std::istringstream rl(requestLine);
    std::string method, path, version;
    rl >> method >> path >> version;

    const size_t q = path.find('?');
    if (q != std::string::npos) {
        path = path.substr(0, q);
    }

    if (HandleApi(client, method, path, body)) {
        closesocket(client);
        return;
    }

    if (method == "GET" && path == "/") {
        const std::wstring indexPath = g_webDir + L"\\index.html";
        const std::string html = ReadFileUtf8(indexPath);
        if (html.empty()) {
            SendHttp(client, 500, "Internal Server Error", "text/plain; charset=utf-8", "index.html missing");
        } else {
            SendHttp(client, 200, "OK", "text/html; charset=utf-8", html);
        }
        closesocket(client);
        return;
    }

    SendHttp(client, 404, "Not Found", "text/plain; charset=utf-8", "not found");
    closesocket(client);
}

bool StartHttpServer()
{
    WSADATA wsa = {};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return false;
    }

    g_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listenSocket == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kServerPort);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    int yes = 1;
    setsockopt(g_listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));

    if (bind(g_listenSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        closesocket(g_listenSocket);
        g_listenSocket = INVALID_SOCKET;
        WSACleanup();
        return false;
    }

    if (listen(g_listenSocket, 16) != 0) {
        closesocket(g_listenSocket);
        g_listenSocket = INVALID_SOCKET;
        WSACleanup();
        return false;
    }

    u_long nonBlocking = 1;
    ioctlsocket(g_listenSocket, FIONBIO, &nonBlocking);

    return true;
}

void StopHttpServer()
{
    if (g_listenSocket != INVALID_SOCKET) {
        closesocket(g_listenSocket);
        g_listenSocket = INVALID_SOCKET;
    }
    WSACleanup();
}

bool WaitForServerReady(int timeoutMs)
{
    const DWORD start = GetTickCount();
    while (static_cast<int>(GetTickCount() - start) < timeoutMs) {
        SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET) {
            Sleep(50);
            continue;
        }

        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(kServerPort);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        const int rc = connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        closesocket(s);
        if (rc == 0) {
            return true;
        }

        Sleep(60);
    }

    return false;
}

void InitDefaultState()
{
    EnsureConfigDirectory();
    const std::wstring defaultPath = BuildProfilePath(L"default.ini");
    CrosshairConfig loaded = LoadConfigFromFile(defaultPath);

    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_config = loaded;
    g_overlayRunning = false;
    g_activeProfile = L"default.ini";
}

void StartOverlayThread()
{
    g_overlayThread = std::thread([]() {
        OverlayThreadProc(nullptr);
    });

    for (int i = 0; i < 200; ++i) {
        if (g_controlWindow != nullptr) {
            break;
        }
        Sleep(10);
    }
}

void StopOverlayThread()
{
    if (g_controlWindow != nullptr) {
        PostMessageW(g_controlWindow, WM_CLOSE, 0, 0);
    }
    if (g_overlayThread.joinable()) {
        g_overlayThread.join();
    }
}

std::wstring FindEdgeExecutable()
{
    const std::vector<std::wstring> candidates = {
        L"C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe",
        L"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe",
    };

    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }

    return L"msedge.exe";
}

bool TryLaunchAppWindow(const std::wstring& edgeExe)
{
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    ZeroMemory(&g_webAppProcess, sizeof(g_webAppProcess));

    // Keep executable path in command line as argv[0], otherwise Chromium can treat the
    // first switch as argv[0] and ignore app-mode arguments.
    std::wstring cmd = L"\"" + edgeExe + L"\" --app=http://127.0.0.1:5188/ --new-window --window-size=1280,860";

    BOOL ok = CreateProcessW(
        nullptr,
        cmd.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        nullptr,
        &si,
        &g_webAppProcess);

    if (!ok) {
        g_lastAppLaunchError = GetLastError();
        return false;
    }

    return true;
}

bool LaunchAppWindow()
{
    g_lastAppLaunchError = 0;
    const std::wstring primary = FindEdgeExecutable();
    if (TryLaunchAppWindow(primary)) {
        return true;
    }

    if (_wcsicmp(primary.c_str(), L"msedge.exe") != 0) {
        if (TryLaunchAppWindow(L"msedge.exe")) {
            return true;
        }
    }

    return false;
}

void StopAppWindow()
{
    if (g_webAppProcess.hProcess != nullptr) {
        const DWORD waitResult = WaitForSingleObject(g_webAppProcess.hProcess, 150);
        if (waitResult == WAIT_TIMEOUT) {
            TerminateProcess(g_webAppProcess.hProcess, 0);
        }
        CloseHandle(g_webAppProcess.hProcess);
        g_webAppProcess.hProcess = nullptr;
    }
    if (g_webAppProcess.hThread != nullptr) {
        CloseHandle(g_webAppProcess.hThread);
        g_webAppProcess.hThread = nullptr;
    }
}
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    g_instance = instance;
    g_exeDir = GetExeDir();
    g_configDir = g_exeDir + L"\\configs";
    g_webDir = g_exeDir + L"\\web";

    InitDefaultState();
    ApplyCommandLineArgs();

    StartOverlayThread();
    NotifyOverlayApply();

    if (!StartHttpServer()) {
        MessageBoxW(nullptr, L"启动 HTTP 服务失败（端口 5188）。", L"MyCross", MB_OK | MB_ICONERROR);
        StopOverlayThread();
        return 1;
    }

    WaitForServerReady(3000);
    g_lastUiPingTick = GetTickCount();

    if (!LaunchAppWindow()) {
        wchar_t msg[256] = {};
        swprintf(msg, 256, L"启动应用窗口失败（错误码: %lu），已回退到默认浏览器。", g_lastAppLaunchError);
        MessageBoxW(nullptr, msg, L"MyCross", MB_OK | MB_ICONWARNING);
        ShellExecuteW(nullptr, L"open", L"http://127.0.0.1:5188/", nullptr, nullptr, SW_SHOWNORMAL);
        g_appWindowMode = false;
    } else {
        g_appWindowMode = true;
    }

    while (!g_exitRequested) {
        if (g_appWindowMode) {
            const DWORD now = GetTickCount();
            const DWORD lastPing = g_lastUiPingTick.load();
            if (now - lastPing > kUiHeartbeatTimeoutMs) {
                g_exitRequested = true;
                break;
            }
        }

        sockaddr_in clientAddr = {};
        int len = sizeof(clientAddr);
        SOCKET client = accept(g_listenSocket, reinterpret_cast<sockaddr*>(&clientAddr), &len);
        if (client == INVALID_SOCKET) {
            const int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                Sleep(40);
                continue;
            }
            if (g_exitRequested) {
                break;
            }
            continue;
        }

        HandleClient(client);
    }

    StopHttpServer();
    StopAppWindow();
    StopOverlayThread();
    return 0;
}
