#pragma once

#include <string>
#include <vector>

#include "app_types.h"

namespace mycross {

void normalize(Config& cfg);
std::wstring profile_name(std::wstring name);
std::wstring profile_path(const AppContext& app, const std::wstring& name);
std::vector<std::wstring> profiles(const AppContext& app);
Config load_cfg(const std::wstring& file);
bool save_cfg(const std::wstring& file, Config cfg);
void ensure_cfg(const AppContext& app);

}  // namespace mycross
