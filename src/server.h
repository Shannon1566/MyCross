#pragma once

#include "app_types.h"

namespace mycross {

    // 初始化本地 HTTP 服务并开始监听。
    bool start_server(AppContext &app);
    // 关闭监听 socket 并清理 Winsock。
    void stop_server(AppContext &app);
    // 在给定超时时间内探测服务是否可连接。
    bool wait_server(int ms);
    // 处理单个客户端请求（同步处理）。
    void handle_client(AppContext &app, SOCKET client);

} // namespace mycross
