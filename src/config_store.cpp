#include "config_store.h"

#include <algorithm>

#include "common.h"

namespace mycross {

void normalize(Config& cfg) {
  cfg.window_size = clampi(cfg.window_size, 20, 800);
  cfg.line_width = clampi(cfg.line_width, 1, 20);
  cfg.cross_half = clampi(cfg.cross_half, 1, std::max(1, cfg.window_size / 2));
  cfg.color_r = clampi(cfg.color_r, 0, 255);
  cfg.color_g = clampi(cfg.color_g, 0, 255);
  cfg.color_b = clampi(cfg.color_b, 0, 255);
  if (cfg.x < -1) {
    cfg.x = -1;
  }
  if (cfg.y < -1) {
    cfg.y = -1;
  }
}

std::wstring profile_name(std::wstring name) {
  name = trim(name);
  for (wchar_t& ch : name) {
    if (ch == L'\\' || ch == L'/' || ch == L':' || ch == L'*' || ch == L'?' ||
        ch == L'"' || ch == L'<' || ch == L'>' || ch == L'|') {
      ch = L'_';
    }
  }
  if (name.empty()) {
    return L"";
  }
  if (name.size() < 4 || _wcsicmp(name.c_str() + name.size() - 4, L".ini") != 0) {
    name += L".ini";
  }
  return name;
}

std::wstring profile_path(const AppContext& app, const std::wstring& name) {
  return app.cfg_dir + L"\\" + name;
}

std::vector<std::wstring> profiles(const AppContext& app) {
  std::vector<std::wstring> result;
  WIN32_FIND_DATAW find_data = {};
  HANDLE handle =
      FindFirstFileW((app.cfg_dir + L"\\*.ini").c_str(), &find_data);
  if (handle == INVALID_HANDLE_VALUE) {
    return result;
  }
  do {
    if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
      result.emplace_back(find_data.cFileName);
    }
  } while (FindNextFileW(handle, &find_data));
  FindClose(handle);
  std::sort(result.begin(), result.end());
  return result;
}

Config load_cfg(const std::wstring& file) {
  Config cfg;
  cfg.x = GetPrivateProfileIntW(L"Crosshair", L"x", cfg.x, file.c_str());
  cfg.y = GetPrivateProfileIntW(L"Crosshair", L"y", cfg.y, file.c_str());
  cfg.window_size =
      GetPrivateProfileIntW(L"Crosshair", L"window_size", cfg.window_size,
                            file.c_str());
  cfg.cross_half =
      GetPrivateProfileIntW(L"Crosshair", L"cross_half", cfg.cross_half,
                            file.c_str());
  cfg.line_width =
      GetPrivateProfileIntW(L"Crosshair", L"line_width", cfg.line_width,
                            file.c_str());
  cfg.color_r =
      GetPrivateProfileIntW(L"Crosshair", L"color_r", cfg.color_r, file.c_str());
  cfg.color_g =
      GetPrivateProfileIntW(L"Crosshair", L"color_g", cfg.color_g, file.c_str());
  cfg.color_b =
      GetPrivateProfileIntW(L"Crosshair", L"color_b", cfg.color_b, file.c_str());
  normalize(cfg);
  return cfg;
}

bool save_cfg(const std::wstring& file, Config cfg) {
  normalize(cfg);
  return WritePrivateProfileStringW(L"Crosshair", L"x", itow(cfg.x).c_str(),
                                    file.c_str()) &&
         WritePrivateProfileStringW(L"Crosshair", L"y", itow(cfg.y).c_str(),
                                    file.c_str()) &&
         WritePrivateProfileStringW(L"Crosshair", L"window_size",
                                    itow(cfg.window_size).c_str(),
                                    file.c_str()) &&
         WritePrivateProfileStringW(L"Crosshair", L"cross_half",
                                    itow(cfg.cross_half).c_str(),
                                    file.c_str()) &&
         WritePrivateProfileStringW(L"Crosshair", L"line_width",
                                    itow(cfg.line_width).c_str(),
                                    file.c_str()) &&
         WritePrivateProfileStringW(L"Crosshair", L"color_r",
                                    itow(cfg.color_r).c_str(),
                                    file.c_str()) &&
         WritePrivateProfileStringW(L"Crosshair", L"color_g",
                                    itow(cfg.color_g).c_str(),
                                    file.c_str()) &&
         WritePrivateProfileStringW(L"Crosshair", L"color_b",
                                    itow(cfg.color_b).c_str(),
                                    file.c_str());
}

void ensure_cfg(const AppContext& app) {
  CreateDirectoryW(app.cfg_dir.c_str(), nullptr);
  if (profiles(app).empty()) {
    save_cfg(profile_path(app, L"default.ini"), Config{});
  }
}

}  // namespace mycross
