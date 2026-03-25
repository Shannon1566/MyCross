#pragma once

#include "app_types.h"

namespace mycross {

    // 创建并显示 WebView2 控制面板窗口。
    bool launch_ui(AppContext &app);
    // 销毁 UI 资源并清理 COM 相关状态。
    void stop_ui(AppContext &app);
    // 解析命令行参数并应用初始配置。
    void apply_cli(AppContext &app);

} // namespace mycross
