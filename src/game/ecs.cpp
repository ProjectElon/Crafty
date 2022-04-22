#include "ecs.h"

namespace minecraft {

    bool ECS::initialize()
    {
        return true;
    }

    void ECS::shutdown()
    {
    }

    ECS_Data ECS::internal_data;
}