#pragma once

#include <string>

namespace mycross {

int clampi(int value, int min_value, int max_value);
int toint(const std::string& s, int default_value);
std::wstring itow(int value);
std::wstring exe_dir();
std::wstring utf8w(const std::string& s);
std::string wutf8(const std::wstring& s);
std::string jesc(const std::string& in);
std::wstring trim(const std::wstring& s);

}  // namespace mycross
