#include "camera.h"
#include "core/input.h"

namespace minecraft {

    static void update_camera_view(Camera *camera)
    {
        glm::vec3 angles = glm::vec3(glm::radians(-camera->pitch), glm::radians(-camera->yaw), 0.0f);
        camera->orientation = glm::quat(angles);
        camera->forward = glm::rotate(camera->orientation, glm::vec3(0.0f, 0.0f, -1.0f));
        camera->right = glm::rotate(camera->orientation, glm::vec3(1.0f, 0.0f, 0.0f));

        camera->up = glm::rotate(camera->orientation, glm::vec3(0.0f, 1.0f, 0.0f));

        glm::mat4 view_model = glm::translate(glm::mat4(1.0f), camera->position) * glm::toMat4(camera->orientation);
        camera->view = glm::inverse(view_model);
    }

    static void update_camera_projection(Camera *camera)
    {
        camera->projection = glm::perspective(glm::radians(camera->fov), camera->aspect_ratio, camera->near, camera->far);
    }

    void initialize_camera(Camera          *camera,
                           const glm::vec3 &position,
                           f32              fov /* = 90.0f*/,
                           f32              aspect_ratio /* = 16.0f / 9.0f */,
                           f32              near /* = 0.1f */,
                           f32              far /* = 1000.0f */)
    {
        camera->position = position;
        camera->yaw = 0.0f;
        camera->pitch = 0.0f;
        camera->fov = fov;
        camera->aspect_ratio = aspect_ratio;
        camera->near = near;
        camera->far = far;
        camera->rotation_speed = 180.0f;
        camera->sensetivity    = 0.5f;

        update_camera_view(camera);
        update_camera_projection(camera);
        camera->frustum.initialize(camera->projection * camera->view);
    }

    void update_camera_transform(Camera *camera,
                                 Input *input,
                                 f32 delta_time)
    {
        glm::vec2 mouse_delta = get_mouse_delta(input);

        camera->pitch += mouse_delta.y * camera->rotation_speed * camera->sensetivity * delta_time;

        if (camera->pitch >= 360.0f) camera->pitch  -= 360.0f;
        if (camera->pitch <= -360.0f) camera->pitch += 360.0f;

        camera->pitch = glm::clamp(camera->pitch, -89.0f, 89.0f);
    }

    void update_camera(Camera *camera)
    {
        update_camera_view(camera);
        update_camera_projection(camera);
        camera->frustum.update(camera->projection * camera->view);
    }
}