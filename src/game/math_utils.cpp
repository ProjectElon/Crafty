#include "math_utils.h"
#include "core/common.h"

namespace minecraft {

    Ray_Cast_Result cast_ray_on_aabb(const Ray& ray, const AABB& aabb)
    {
        f32 t1 = (aabb.min.x - ray.origin.x) / ray.direction.x;
        f32 t2 = (aabb.max.x - ray.origin.x) / ray.direction.x;
        f32 t3 = (aabb.min.y - ray.origin.y) / ray.direction.y;
        f32 t4 = (aabb.max.y - ray.origin.y) / ray.direction.y;
        f32 t5 = (aabb.min.z - ray.origin.z) / ray.direction.z;
        f32 t6 = (aabb.max.z - ray.origin.z) / ray.direction.z;

        f32 tmin = glm::max(glm::max(glm::min(t1, t2), glm::min(t3, t4)), glm::min(t5, t6));
        f32 tmax = glm::min(glm::min(glm::max(t1, t2), glm::max(t3, t4)), glm::max(t5, t6));

        Ray_Cast_Result result {};

        if (tmax < 0.0f || tmin > tmax)
        {
            return result;
        }

        result.hit = true;

        if (tmin < 0.0f)
        {
            result.point = ray.origin + tmax * ray.direction;
        }

        result.point = ray.origin + tmin * ray.direction;
        return result;
    }
}