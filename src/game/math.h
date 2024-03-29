#pragma once

#include "core/common.h"

#include <numeric>

#include <glm/glm.hpp>

namespace minecraft {

    #define Infinity32 std::numeric_limits<f32>::max()

    struct Rectanglei
    {
        i32 x;
        i32 y;
        u32 width;
        u32 height;
    };

    struct Rectangle
    {
        glm::vec2 min;
        glm::vec2 max;
    };

    struct Texture_Coords
    {
        glm::vec2 scale;
        glm::vec2 offset;
    };

    // todo(harlequin): remove the UV_Rectangle
    struct UV_Rectangle
    {
        glm::vec2 bottom_right;
        glm::vec2 bottom_left;
        glm::vec2 top_left;
        glm::vec2 top_right;
    };

    Rectangle rectangle(const glm::vec2& top_left,
                        const glm::vec2& size);

    Rectangle rectangle(f32 x,
                        f32 y,
                        f32 width,
                        f32 height);

    Rectangle rectangle_min_max(const glm::vec2& min,
                                const glm::vec2& max);

    bool is_point_inside_rectangle(const glm::vec2& point,
                                   const Rectangle& rectangle);

    bool is_rectangle_inside_rectangle(const Rectanglei& a,
                                       const Rectanglei& b);

    bool is_rectangle_inside_rectangle(const Rectangle& a,
                                       const Rectangle& b);

    UV_Rectangle convert_texture_rect_to_uv_rect(const Rectanglei &rect,
                                                 u32 texture_width,
                                                 u32 texture_height);

     Texture_Coords rectangle_to_texture_coords(const Rectanglei &rect,
                                                u32 texture_width,
                                                u32 texture_height);

    struct AABB
    {
        glm::vec3 min;
        glm::vec3 max;
    };

    struct Ray
    {
        glm::vec3 origin;
        glm::vec3 direction;
    };

    struct Ray_Cast_Result
    {
        bool      hit;
        glm::vec3 point;
        f32       distance;
    };

    Ray_Cast_Result cast_ray_on_aabb(const Ray& ray,
                                     const AABB& aabb,
                                     f32 max_distance = Infinity32);

    struct Plane
    {
        glm::vec3 point;
        glm::vec3 normal;
    };

    struct Frustum
    {
        enum Planes
        {
            Left = 0,
            Right,
            Bottom,
            Top,
            Near,
            Far,
            Count,
            Combinations = Count * (Count - 1) / 2
        };

        glm::vec4 planes[Count];
        glm::vec3 points[8];

        void initialize(const glm::mat4& camera_projection_mul_view);

        void update(const glm::mat4& camera_projection_mul_view);
        bool is_aabb_visible(const AABB& aabb) const;

    private:
        template< Planes i, Planes j >
        struct ij2k
        {
            enum { k = i * (9 - i) / 2 + j - 1 };
        };

        template<Planes a, Planes b, Planes c>
        glm::vec3 intersection(const glm::vec3* crosses) const
        {
            float D = glm::dot(glm::vec3(planes[a]), crosses[ij2k<b, c>::k]);
            glm::vec3 res = glm::mat3(crosses[ij2k<b, c>::k], -crosses[ij2k<a, c>::k], crosses[ij2k<a, b>::k]) *
                glm::vec3(planes[a].w, planes[b].w, planes[c].w);
            return res * (-1.0f / D);
        }
    };
}
