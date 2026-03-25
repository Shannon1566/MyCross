#pragma once

#include <string>
#include <vector>

#include "app_types.h"

namespace mycross {

    // 规范化配置范围，避免非法值。
    void normalize(Config &cfg);
    // 清洗并规范 profile 文件名（补齐 .ini）。
    std::wstring profile_name(std::wstring name);
    // 拼接 profile 的绝对路径。
    std::wstring profile_path(const AppContext &app, const std::wstring &name);
    // 列出 configs 目录下所有 profile。
    std::vector<std::wstring> profiles(const AppContext &app);
    // 从 INI 读取配置。
    Config load_cfg(const std::wstring &file);
    // 将配置写入 INI。
    bool save_cfg(const std::wstring &file, Config cfg);
    // 确保配置目录与默认配置存在。
    void ensure_cfg(const AppContext &app);

} // namespace mycross
