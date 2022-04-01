#pragma once

#include "core/common.h"

#include <glm/glm.hpp>

namespace minecraft {

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
        bool hit;
        glm::vec3 point;
    };

    Ray_Cast_Result cast_ray_on_aabb(const Ray& ray, const AABB& aabb);

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

        template<Planes i, Planes j>
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
