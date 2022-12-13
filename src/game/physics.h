#pragma once

#include "core/common.h"
#include "game/components.h"
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

    struct Physics_Data
    {
        glm::vec3 gravity = { 0.0f, 20.0f, 0.0f };
        i32 update_rate;
        f32 delta_time;
        f32 delta_time_accumulator;
    };

    struct Registry;
    struct World;

    struct Physics
    {
        static Physics_Data internal_data;

        static bool initialize(i32 update_rate);
        static void shutdown();
        static void simulate(f32 delta_time, World *world, Registry *registry);

        static bool box_vs_box(const Transform& t0, const Box_Collider& c0, const Transform& t1, const Box_Collider& c1);
        static bool is_colliding(const Transform& t0, const Box_Collider& c0, const Transform& t1, const Box_Collider& c1);
        static Box_Vs_Box_Collision_Info get_static_collision_information(const Transform& t0, const Box_Collider& c0, const Transform& t1, const Box_Collider& c1);

        static Box_Vs_Box_Collision_Info resolve_dynamic_box_vs_static_box_collision(Rigid_Body& rb,
                                                                                     Transform& t0,
                                                                                     Box_Collider& bc0,
                                                                                     const Transform& t1,
                                                                                     const Box_Collider& bc1);
    };
}
