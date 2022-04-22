#pragma once
#include "core/common.h"
#include <glm/glm.hpp>

namespace minecraft {

    struct Transform_Component
    {
        glm::vec3 position;
        glm::vec3 scale;
        glm::vec3 rotation;
    };

    struct Box_Collider_Component
    {
        glm::vec3 size;
        glm::vec3 offset;
    };

    struct Rigid_Body_Component
    {
        glm::vec3 acceleration;
        glm::vec3 velocity;
        bool is_grounded;
    };
}