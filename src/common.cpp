#include "common.h"

#include <algorithm>
#include <cwctype>

#include <windows.h>

namespace mycross {

    // 通用整数区间裁剪。
    int clampi(int value, int min_value, int max_value) {
        return std::max(min_value, std::min(value, max_value));
    }

    // 仅接受完整十进制字符串；否则回退到默认值。
    int toint(const std::string &s, int default_value) {
        if (s.empty()) {
            return default_value;
        }
        char *end = nullptr;
        long value = strtol(s.c_str(), &end, 10);
        return (end == s.c_str() || *end != '\0') ? default_value
                                                  : static_cast<int>(value);
    }

    // 轻量整数转宽字符串，避免引入流对象开销。
    std::wstring itow(int value) {
        wchar_t buffer[32] = {};
        swprintf(buffer, 32, L"%d", value);
        return buffer;
    }

    // 读取当前模块路径并截取目录部分。
    std::wstring exe_dir() {
        wchar_t path[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring result(path);
        const size_t sep = result.find_last_of(L"\\/");
        return sep == std::wstring::npos ? L"." : result.substr(0, sep);
    }

    // UTF-8 -> UTF-16，失败返回空串。
    std::wstring utf8w(const std::string &s) {
        if (s.empty()) {
            return L"";
        }
        const int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        if (size <= 0) {
            return L"";
        }
        std::wstring out(static_cast<size_t>(size) - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), size);
        return out;
    }

    // UTF-16 -> UTF-8，失败返回空串。
    std::string wutf8(const std::wstring &s) {
        if (s.empty()) {
            return "";
        }
        const int size =
            WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (size <= 0) {
            return "";
        }
        std::string out(static_cast<size_t>(size) - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, out.data(), size, nullptr,
                            nullptr);
        return out;
    }

    // 仅处理当前项目所需的常见 JSON 转义字符。
    std::string jesc(const std::string &in) {
        std::string out;
        for (char ch : in) {
            if (ch == '\\') {
                out += "\\\\";
            } else if (ch == '"') {
                out += "\\\"";
            } else if (ch == '\n') {
                out += "\\n";
            } else if (ch == '\r') {
                out += "\\r";
            } else if (ch == '\t') {
                out += "\\t";
            } else {
                out += ch;
            }
        }
        return out;
    }

    // 去除字符串首尾空白，不修改中间内容。
    std::wstring trim(const std::wstring &s) {
        size_t left = 0;
        while (left < s.size() && iswspace(s[left])) {
            ++left;
        }
        size_t right = s.size();
        while (right > left && iswspace(s[right - 1])) {
            --right;
        }
        return s.substr(left, right - left);
    }

} // namespace mycross
