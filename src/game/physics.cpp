#include "game/physics.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "game/ecs.h"
#include "game/world.h"

#include <optick/optick.h>

namespace minecraft {

    bool Physics::initialize(i32 update_rate)
    {
        OPTICK_EVENT();
        internal_data.update_rate = update_rate;
        internal_data.delta_time = 1.0f / (f32)update_rate;
        return true;
    }

    void Physics::shutdown()
    {
        OPTICK_EVENT();
    }

    void Physics::simulate(f32 delta_time, World *world, Registry *registry)
    {
        auto& delta_time_accumulator = internal_data.delta_time_accumulator;
        auto& physics_delta_time = internal_data.delta_time;
        delta_time_accumulator += delta_time;

        while (delta_time_accumulator >= physics_delta_time)
        {
            auto view = get_view<Transform, Box_Collider, Rigid_Body>(registry);

            for (auto entity = view.begin(); entity != view.end(); entity = view.next(entity))
            {
                auto [transform, box_collider, rb] = registry->get_components< Transform, Box_Collider, Rigid_Body >(entity);

                Character_Controller *controller = registry->get_component< Character_Controller >(entity);

                if (controller)
                {
                    rb->velocity.x = controller->movement.x * controller->movement_speed;
                    rb->velocity.z = controller->movement.z * controller->movement_speed;
                }

                transform->position += rb->velocity * physics_delta_time;
                rb->velocity += rb->acceleration * physics_delta_time;

                rb->velocity -= Physics::internal_data.gravity * physics_delta_time;

                if (controller) rb->velocity = glm::clamp(rb->velocity, -controller->terminal_velocity, controller->terminal_velocity);

                glm::vec3 box_collider_half_size = box_collider->size * 0.5f;
                glm::ivec3 min = glm::ceil(transform->position - box_collider_half_size);
                glm::ivec3 max = glm::ceil(transform->position + box_collider_half_size);

                bool collide = false;
                rb->is_under_water = false;

                for (i32 y = max.y; y >= min.y; --y)
                {
                    for (i32 z = min.z; z <= max.z; ++z)
                    {
                        for (i32 x = min.x; x <= max.x; ++x)
                        {
                            glm::vec3 block_pos = glm::vec3(x - 0.5f, y - 0.5f, z - 0.5f);
                            auto query = query_block(world, block_pos);
                            if (is_block_query_valid(query))
                            {
                                const Block_Info *block_info = get_block_info(world, query.block);
                                if (is_block_solid(block_info))
                                {
                                    Transform block_transform = {};
                                    block_transform.position = block_pos;
                                    block_transform.scale = { 1.0f, 1.0f, 1.0f };
                                    block_transform.orientation = { 0.0f, 0.0f, 0.0f };

                                    Box_Collider block_collider = {};
                                    block_collider.size = { 1.0f, 1.0f, 1.0f };

                                    if (Physics::is_colliding(*transform, *box_collider, block_transform, block_collider))
                                    {
                                        Box_Vs_Box_Collision_Info info = Physics::resolve_dynamic_box_vs_static_box_collision(*rb,
                                                                                                                              *transform,
                                                                                                                              *box_collider,
                                                                                                                              block_transform,
                                                                                                                              block_collider);
                                        if (controller) controller->is_grounded = controller->is_grounded || info.face == CollisionFace_Bottom;
                                        collide = true;
                                    }
                                }

                                if (query.block->id == BlockId_Water)
                                {
                                    rb->is_under_water = true;
                                }
                            }
                        }
                    }
                }

                if (!collide && controller && controller->is_grounded && rb->velocity.y > 0)
                {
                    // If we're not colliding with any object it's impossible to be on the ground
                    controller->is_grounded = false;
                }
            }

            delta_time_accumulator -= physics_delta_time;
        }
    }

    bool Physics::box_vs_box(const Transform& t0, const Box_Collider& c0, const Transform& t1, const Box_Collider& c1)
    {
        OPTICK_EVENT();
        glm::vec3 min0 = t0.position - (c0.size * 0.5f);
        glm::vec3 max0 = t0.position + (c0.size * 0.5f);

        glm::vec3 min1 = t1.position - (c1.size * 0.5f);
        glm::vec3 max1 = t1.position + (c1.size * 0.5f);

        return (min0.x <= max1.x && max0.x >= min1.x) &&
               (min0.y <= max1.y && max0.y >= min1.y) &&
               (min0.z <= max1.z && max0.z >= min1.z);
    }

    bool Physics::is_colliding(const Transform& t0, const Box_Collider& c0, const Transform& t1, const Box_Collider& c1)
    {
        OPTICK_EVENT();
        glm::vec3 min0 = t0.position - (c0.size * 0.5f);
        glm::vec3 max0 = t0.position + (c0.size * 0.5f);

        glm::vec3 min1 = t1.position - (c1.size * 0.5f);
        glm::vec3 max1 = t1.position + (c1.size * 0.5f);

        if ((min1.x <= max0.x) && (min0.x <= max1.x))
        {
            f32 xp = min1.x - max0.x;
            if (glm::abs(xp) <= 0.001f) return false;
        }

        if ((min1.y <= max0.y) && (min0.y <= max1.y))
        {
            f32 yp = max1.y - min0.y;
            if (glm::abs(yp) <= 0.001f) return false;
        }

        if ((min1.z <= max0.z) && (min0.z <= max1.z))
        {
            f32 zp = min1.z - max0.z;
            if (glm::abs(zp) <= 0.001f) return false;
        }

        return true;
    }

    static f32 get_collision_direction(CollisionFace face)
    {
        OPTICK_EVENT();
        switch (face)
        {
            case CollisionFace_Back:
                return 1;
            case CollisionFace_Front:
                return -1;
            case CollisionFace_Right:
                return -1;
            case CollisionFace_Left:
                return 1;
            case CollisionFace_Top:
                return -1;
            case CollisionFace_Bottom:
                return 1;
        }
    }

    static void get_quadrant_result(const Transform& t0,
                                    const Transform& t1,
                                    const Box_Collider& b1_expanded,
                                    CollisionFace x_face,
                                    CollisionFace y_face,
                                    CollisionFace z_face,
                                    Box_Vs_Box_Collision_Info* out_info)
    {
        OPTICK_EVENT();
        f32 x_dir = get_collision_direction(x_face);
        f32 y_dir = get_collision_direction(y_face);
        f32 z_dir = get_collision_direction(z_face);

        glm::vec3 expended_size_by_dir = { b1_expanded.size.x * x_dir, b1_expanded.size.y * y_dir, b1_expanded.size.z * z_dir };
        glm::vec3 quadrant = (expended_size_by_dir * 0.5f) + t1.position;
        glm::vec3 delta = t0.position - quadrant;
        glm::vec3 abs_delta = glm::abs(delta);

        if (abs_delta.x < abs_delta.y && abs_delta.x < abs_delta.z)
        {
            out_info->overlap = delta.x * glm::vec3(1.0f, 0.0f, 0.0f);
            out_info->face = x_face;
        }
        else if (abs_delta.y < abs_delta.x && abs_delta.y < abs_delta.z)
        {
            out_info->overlap = delta.y * glm::vec3(0.0f, 1.0f, 0.0f);
            out_info->face = y_face;
        }
        else
        {
            out_info->overlap = delta.z * glm::vec3(0.0f, 0.0f, 1.0f);
            out_info->face = z_face;
        }
    }

    Box_Vs_Box_Collision_Info Physics::get_static_collision_information(const Transform& t0, const Box_Collider& c0, const Transform& t1, const Box_Collider& c1)
    {
        OPTICK_EVENT();
        Box_Vs_Box_Collision_Info collision_info;

        Box_Collider b1_expanded = c1;
        b1_expanded.size += c0.size;

        glm::vec3 p0_to_p1 = t0.position - t1.position;

        if (p0_to_p1.x > 0.0f && p0_to_p1.y > 0.0f && p0_to_p1.z > 0.0f) // right top front quadrant
        {
            get_quadrant_result(t0, t1, b1_expanded, CollisionFace_Left, CollisionFace_Bottom, CollisionFace_Back, &collision_info);
        }
        else if (p0_to_p1.x > 0.0f && p0_to_p1.y > 0.0f && p0_to_p1.z <= 0.0f) // right top back quadrant
        {
            get_quadrant_result(t0, t1, b1_expanded, CollisionFace_Left, CollisionFace_Bottom, CollisionFace_Front, &collision_info);
        }
        else if (p0_to_p1.x > 0 && p0_to_p1.y <= 0 && p0_to_p1.z > 0) // right bottom front quadrant
        {
            get_quadrant_result(t0, t1, b1_expanded, CollisionFace_Left, CollisionFace_Top, CollisionFace_Back, &collision_info);
        }
        else if (p0_to_p1.x > 0 && p0_to_p1.y <= 0 && p0_to_p1.z <= 0) // right bottom back quadrant
        {
            get_quadrant_result(t0, t1, b1_expanded, CollisionFace_Left, CollisionFace_Top, CollisionFace_Front, &collision_info);
        }
        else if (p0_to_p1.x <= 0 && p0_to_p1.y > 0 && p0_to_p1.z > 0) // left top front quadrant
        {
            get_quadrant_result(t0, t1, b1_expanded, CollisionFace_Right, CollisionFace_Bottom, CollisionFace_Back, &collision_info);
        }
        else if (p0_to_p1.x <= 0 && p0_to_p1.y > 0 && p0_to_p1.z <= 0) // left top back quadrant
        {
            get_quadrant_result(t0, t1, b1_expanded, CollisionFace_Right, CollisionFace_Bottom, CollisionFace_Front, &collision_info);
        }
        else if (p0_to_p1.x <= 0 && p0_to_p1.y <= 0 && p0_to_p1.z > 0) // left bottom front quadrant
        {
            get_quadrant_result(t0, t1, b1_expanded, CollisionFace_Right, CollisionFace_Top, CollisionFace_Back, &collision_info);
        }
        else if (p0_to_p1.x <= 0 && p0_to_p1.y <= 0 && p0_to_p1.z <= 0) // left bottom back quadrant
        {
            get_quadrant_result(t0, t1, b1_expanded, CollisionFace_Right, CollisionFace_Top, CollisionFace_Front, &collision_info);
        }

        return collision_info;
    }


    Box_Vs_Box_Collision_Info Physics::resolve_dynamic_box_vs_static_box_collision(Rigid_Body& rb,
                                                                                   Transform& t0,
                                                                                   Box_Collider& bc0,
                                                                                   const Transform& t1,
                                                                                   const Box_Collider& bc1)
    {
        OPTICK_EVENT();
        Box_Vs_Box_Collision_Info collision_info = Physics::get_static_collision_information(t0, bc0, t1, bc1);
        f32 dot = glm::dot(glm::normalize(collision_info.overlap), glm::normalize(rb.velocity));
        // We're already moving out of the collision, don't do anything
        if (dot < 0.0f)
        {
            return collision_info;
        }
        t0.position -= collision_info.overlap;
        switch (collision_info.face)
        {
            case CollisionFace_Left:
            case CollisionFace_Right:
            {
                rb.acceleration.x = 0.0f;
                rb.velocity.x = 0.0f;
            } break;

            case CollisionFace_Top:
            case CollisionFace_Bottom:
            {
                rb.acceleration.y = 0.0f;
                rb.velocity.y = 0.0f;
            } break;

            case CollisionFace_Front:
            case CollisionFace_Back:
            {
                rb.acceleration.z = 0.0f;
                rb.velocity.z = 0.0f;
            } break;
        }
        return collision_info;
    }

    Physics_Data Physics::internal_data;
}