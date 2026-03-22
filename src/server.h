#pragma once

#include "app_types.h"

namespace mycross {

    bool start_server(AppContext &app);
    void stop_server(AppContext &app);
    bool wait_server(int ms);
    void handle_client(AppContext &app, SOCKET client);

} // namespace mycross
