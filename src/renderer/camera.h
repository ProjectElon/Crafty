#pragma once

#include "core/common.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace minecraft {

    struct Camera
    {
        glm::vec3 position;

        f32 walk_speed;
        f32 run_speed;
        f32 movment_speed;
        f32 rotation_speed;
        f32 sensetivity;

        f32 yaw;
        f32 pitch;

        f32 fov;
        f32 aspect_ratio;
        f32 near;
        f32 far;

        glm::quat orientation;
        glm::vec3 forward;
        glm::vec3 right;
        glm::vec3 up;

        glm::mat4 view;
        glm::mat4 projection;

        void initialize(const glm::vec3& position, f32 fov = 45.0f, f32 aspect_ratio = 16.0f / 9.0f, f32 near = 0.01f, f32 far = 1000.0f);
        void update(f32 delta_time);

        void update_view();
        void update_projection();
    };
}