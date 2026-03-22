#include "common.h"

#include <algorithm>
#include <cwctype>

#include <windows.h>

namespace mycross {

int clampi(int value, int min_value, int max_value) {
  return std::max(min_value, std::min(value, max_value));
}

int toint(const std::string& s, int default_value) {
  if (s.empty()) {
    return default_value;
  }
  char* end = nullptr;
  long value = strtol(s.c_str(), &end, 10);
  return (end == s.c_str() || *end != '\0') ? default_value
                                            : static_cast<int>(value);
}

std::wstring itow(int value) {
  wchar_t buffer[32] = {};
  swprintf(buffer, 32, L"%d", value);
  return buffer;
}

std::wstring exe_dir() {
  wchar_t path[MAX_PATH] = {};
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  std::wstring result(path);
  const size_t sep = result.find_last_of(L"\\/");
  return sep == std::wstring::npos ? L"." : result.substr(0, sep);
}

std::wstring utf8w(const std::string& s) {
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

std::string wutf8(const std::wstring& s) {
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

std::string jesc(const std::string& in) {
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

std::wstring trim(const std::wstring& s) {
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

}  // namespace mycross
