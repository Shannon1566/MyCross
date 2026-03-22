#pragma once

#include "app_types.h"

namespace mycross {

    void start_overlay(AppContext &app);
    void stop_overlay(AppContext &app);
    void apply_overlay(AppContext &app);
    void post_sync(const AppContext &app);

} // namespace mycross
