#include "game/physics.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace minecraft {

    bool Physics::initialize(i32 update_rate)
    {
        internal_data.update_rate = update_rate;
        internal_data.delta_time = 1.0f / (f32)update_rate;
        return true;
    }

    void Physics::shutdown()
    {
    }

    bool Physics::box_vs_box(const Transform& t0, const Box_Collider& c0, const Transform& t1, const Box_Collider& c1)
    {
        glm::vec3 min0 = t0.position - c0.size * 0.5f;
        glm::vec3 max0 = t0.position + c0.size * 0.5f;

        glm::vec3 min1 = t1.position - c1.size * 0.5f;
        glm::vec3 max1 = t1.position + c1.size * 0.5f;

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

    static f32 get_box_vs_box_collision_direction(CollisionFace face)
    {
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
        f32 x_dir = get_box_vs_box_collision_direction(x_face);
        f32 y_dir = get_box_vs_box_collision_direction(y_face);
        f32 z_dir = get_box_vs_box_collision_direction(z_face);

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

    Box_Vs_Box_Collision_Info Physics::get_box_vs_box_static_collision_information(const Transform& t0, const Box_Collider& c0, const Transform& t1, const Box_Collider& c1)
    {
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


    void Physics::resolve_dynamic_box_vs_static_box_collision(Rigid_Body& rb,
                                                              Transform& t0,
                                                              Box_Collider& bc0,
                                                              const Transform& t1,
                                                              const Box_Collider& bc1)
    {
        Box_Vs_Box_Collision_Info collision_info = Physics::get_box_vs_box_static_collision_information(t0, bc0, t1, bc1);
        f32 dot = glm::dot(glm::normalize(collision_info.overlap), glm::normalize(rb.velocity));
        // We're already moving out of the collision, don't do anything
        if (dot < 0.0f)
        {
            return;
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

        rb.is_grounded = rb.is_grounded || collision_info.face == CollisionFace_Bottom;
    }

    Physics_Data Physics::internal_data;
}