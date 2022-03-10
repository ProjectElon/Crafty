#pragma once

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
}
