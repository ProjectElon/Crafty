#pragma once
#include "core/common.h"
#include "game/components.h"
#include <variant>

namespace minecraft {

    using Entity = u32;

    struct ECS_Data
    {
    };

    struct ECS
    {
        static ECS_Data internal_data;

        static bool initialize();
        static void shutdown();
    };
}
