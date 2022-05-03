#pragma once
#include "core/common.h"
#include <glm/glm.hpp>

namespace minecraft {

    struct Transform
    {
        glm::vec3 position;
        glm::vec3 scale;
        glm::vec3 orientation;
    };

    struct Box_Collider
    {
        glm::vec3 size;
        glm::vec3 offset;
    };

    struct Rigid_Body
    {
        glm::vec3 acceleration;
        glm::vec3 velocity;
    };

    struct Character_Controller
    {
        f32 movement_speed;
        f32 walk_speed;
        f32 run_speed;
        f32 jump_force;
        f32 fall_force;
        f32 turn_speed;
        f32 sensetivity;

        glm::vec3 movement;
        glm::vec3 terminal_velocity;
        bool is_grounded;
        bool is_jumping;
        bool is_running;
    };
}