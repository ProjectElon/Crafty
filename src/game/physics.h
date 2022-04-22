#pragma once

#include "core/common.h"
#include <glm/glm.hpp>

namespace minecraft {

    enum CollisionFace
    {
        CollisionFace_None,
        CollisionFace_Top,
        CollisionFace_Bottom,
        CollisionFace_Left,
        CollisionFace_Right,
        CollisionFace_Front,
        CollisionFace_Back
    };

    struct Box_Vs_Box_Collision_Info
    {
        glm::vec3     overlap;
        CollisionFace face;
    };

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
        bool is_grounded;
    };

    struct Physics_Data
    {
        i32 update_rate;
        f32 delta_time;
    };

    struct Physics
    {
        static Physics_Data internal_data;

        static bool initialize(i32 update_rate);
        static void shutdown();

        static bool box_vs_box(const Transform& t0, const Box_Collider& c0, const Transform& t1, const Box_Collider& c1);
        static Box_Vs_Box_Collision_Info get_box_vs_box_static_collision_information(const Transform& t0, const Box_Collider& c0, const Transform& t1, const Box_Collider& c1);

        static void resolve_dynamic_box_vs_static_box_collision(Rigid_Body& rb,
                                                                Transform& t0,
                                                                Box_Collider& bc0,
                                                                const Transform& t1,
                                                                const Box_Collider& bc1);
    };
}
