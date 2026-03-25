#pragma once

#include "app_types.h"

namespace mycross {

    // 启动覆盖层线程与控制窗口。
    void start_overlay(AppContext &app);
    // 停止覆盖层线程并回收窗口。
    void stop_overlay(AppContext &app);
    // 按当前状态创建/销毁/重绘准星窗口。
    void apply_overlay(AppContext &app);
    // 向覆盖层线程投递一次同步消息。
    void post_sync(const AppContext &app);

} // namespace mycross
