#pragma once

#include <string>

namespace mycross {

    // 将 value 约束到 [min_value, max_value] 区间。
    int clampi(int value, int min_value, int max_value);
    // 安全地将字符串转为 int，失败时返回 default_value。
    int toint(const std::string &s, int default_value);
    // 将 int 转换为宽字符串。
    std::wstring itow(int value);
    // 获取当前可执行文件所在目录。
    std::wstring exe_dir();
    // UTF-8 std::string -> UTF-16 std::wstring。
    std::wstring utf8w(const std::string &s);
    // UTF-16 std::wstring -> UTF-8 std::string。
    std::string wutf8(const std::wstring &s);
    // 对字符串做 JSON 转义。
    std::string jesc(const std::string &in);
    // 去除宽字符串首尾空白字符。
    std::wstring trim(const std::wstring &s);

} // namespace mycross
