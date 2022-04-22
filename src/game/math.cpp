#include "game/math.h"
#include "core/common.h"
#include "renderer/camera.h"

namespace minecraft {

    Ray_Cast_Result cast_ray_on_aabb(const Ray& ray, const AABB& aabb, f32 max_distance)
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

        if (tmin < 0.0f)
        {
            result.point = ray.origin + tmax * ray.direction;
        }

        result.point = ray.origin + tmin * ray.direction;

        result.distance = glm::length(ray.origin - result.point);

        if (result.distance <= max_distance)
        {
            result.hit = true;
        }

        return result;
    }


    void Frustum::initialize(const glm::mat4& camera_projection_mul_view)
    {
        update(camera_projection_mul_view);
    }

    void Frustum::update(const glm::mat4& camera_view_mul_projection)
    {
        glm::mat4 transposedM = glm::transpose(camera_view_mul_projection);
        planes[Left] = transposedM[3] + transposedM[0];
        planes[Right] = transposedM[3] - transposedM[0];
        planes[Bottom] = transposedM[3] + transposedM[1];
        planes[Top] = transposedM[3] - transposedM[1];
        planes[Near] = transposedM[3] + transposedM[2];
        planes[Far] = transposedM[3] - transposedM[2];

        glm::vec3 crosses[Combinations] = {
            glm::cross(glm::vec3(planes[Left]),   glm::vec3(planes[Right])),
            glm::cross(glm::vec3(planes[Left]),   glm::vec3(planes[Bottom])),
            glm::cross(glm::vec3(planes[Left]),   glm::vec3(planes[Top])),
            glm::cross(glm::vec3(planes[Left]),   glm::vec3(planes[Near])),
            glm::cross(glm::vec3(planes[Left]),   glm::vec3(planes[Far])),
            glm::cross(glm::vec3(planes[Right]),  glm::vec3(planes[Bottom])),
            glm::cross(glm::vec3(planes[Right]),  glm::vec3(planes[Top])),
            glm::cross(glm::vec3(planes[Right]),  glm::vec3(planes[Near])),
            glm::cross(glm::vec3(planes[Right]),  glm::vec3(planes[Far])),
            glm::cross(glm::vec3(planes[Bottom]), glm::vec3(planes[Top])),
            glm::cross(glm::vec3(planes[Bottom]), glm::vec3(planes[Near])),
            glm::cross(glm::vec3(planes[Bottom]), glm::vec3(planes[Far])),
            glm::cross(glm::vec3(planes[Top]),    glm::vec3(planes[Near])),
            glm::cross(glm::vec3(planes[Top]),    glm::vec3(planes[Far])),
            glm::cross(glm::vec3(planes[Near]),   glm::vec3(planes[Far]))
        };

        points[0] = intersection<Left, Bottom, Near>(crosses);
        points[1] = intersection<Left, Top, Near>(crosses);
        points[2] = intersection<Right, Bottom, Near>(crosses);
        points[3] = intersection<Right, Top, Near>(crosses);
        points[4] = intersection<Left, Bottom, Far>(crosses);
        points[5] = intersection<Left, Top, Far>(crosses);
        points[6] = intersection<Right, Bottom, Far>(crosses);
        points[7] = intersection<Right, Top, Far>(crosses);
    }

    // http://iquilezles.org/www/articles/frustumcorrect/frustumcorrect.htm
    bool Frustum::is_aabb_visible(const AABB& aabb) const
    {
        const glm::vec3& minp = aabb.min;
        const glm::vec3& maxp = aabb.max;

        // check box outside/inside of frustum
        for (int i = 0; i < Count; i++)
        {
            if ((glm::dot(planes[i], glm::vec4(minp.x, minp.y, minp.z, 1.0f)) < 0.0) &&
                (glm::dot(planes[i], glm::vec4(maxp.x, minp.y, minp.z, 1.0f)) < 0.0) &&
                (glm::dot(planes[i], glm::vec4(minp.x, maxp.y, minp.z, 1.0f)) < 0.0) &&
                (glm::dot(planes[i], glm::vec4(maxp.x, maxp.y, minp.z, 1.0f)) < 0.0) &&
                (glm::dot(planes[i], glm::vec4(minp.x, minp.y, maxp.z, 1.0f)) < 0.0) &&
                (glm::dot(planes[i], glm::vec4(maxp.x, minp.y, maxp.z, 1.0f)) < 0.0) &&
                (glm::dot(planes[i], glm::vec4(minp.x, maxp.y, maxp.z, 1.0f)) < 0.0) &&
                (glm::dot(planes[i], glm::vec4(maxp.x, maxp.y, maxp.z, 1.0f)) < 0.0))
            {
                return false;
            }
        }

        // check frustum outside/inside box
        int out;
        out = 0; for (int i = 0; i < 8; i++) out += ((points[i].x > maxp.x) ? 1 : 0); if (out == 8) return false;
        out = 0; for (int i = 0; i < 8; i++) out += ((points[i].x < minp.x) ? 1 : 0); if (out == 8) return false;
        out = 0; for (int i = 0; i < 8; i++) out += ((points[i].y > maxp.y) ? 1 : 0); if (out == 8) return false;
        out = 0; for (int i = 0; i < 8; i++) out += ((points[i].y < minp.y) ? 1 : 0); if (out == 8) return false;
        out = 0; for (int i = 0; i < 8; i++) out += ((points[i].z > maxp.z) ? 1 : 0); if (out == 8) return false;
        out = 0; for (int i = 0; i < 8; i++) out += ((points[i].z < minp.z) ? 1 : 0); if (out == 8) return false;

        return true;
    }
}