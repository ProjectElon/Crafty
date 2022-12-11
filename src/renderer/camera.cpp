#include "camera.h"
#include "core/input.h"

namespace minecraft {

    void Camera::initialize(const glm::vec3& position, f32 fov, f32 aspect_ratio, f32 near, f32 far)
    {
        this->position = position;
        this->walk_speed = 15.0f;
        this->run_speed = 25.0f;
        this->movment_speed = this->walk_speed;
        this->rotation_speed = 180.0f;
        this->sensetivity = 0.5f;
        this->yaw = 0.0f;
        this->pitch = 0.0f;
        this->fov = fov;
        this->aspect_ratio = aspect_ratio;
        this->near = near;
        this->far = far;
        
        update_view();
        update_projection();

        this->frustum.initialize(this->projection * this->view);
    }

    void Camera::update_transform(Input *input, f32 delta_time)
    {
        glm::vec2 mouse_delta = get_mouse_delta(input);

        this->pitch += mouse_delta.y * this->rotation_speed * this->sensetivity * delta_time;

        if (this->pitch >= 360.0f) this->pitch  -= 360.0f;
        if (this->pitch <= -360.0f) this->pitch += 360.0f;

        this->pitch = glm::clamp(this->pitch, -89.0f, 89.0f);
    }

    void Camera::update()
    {
        update_view();
        update_projection();
        this->frustum.update(this->projection * this->view);
    }

    void Camera::update_view()
    {
        glm::vec3 angles = glm::vec3(glm::radians(-this->pitch), glm::radians(-this->yaw), 0.0f);
        this->orientation = glm::quat(angles);
        this->forward = glm::rotate(this->orientation, glm::vec3(0.0f, 0.0f, -1.0f));
        this->right = glm::rotate(this->orientation, glm::vec3(1.0f, 0.0f, 0.0f));

        this->up = glm::rotate(this->orientation, glm::vec3(0.0f, 1.0f, 0.0f));

        glm::mat4 view_model = glm::translate(glm::mat4(1.0f), this->position) * glm::toMat4(this->orientation);
        this->view = glm::inverse(view_model);
    }

    void Camera::update_projection()
    {
        this->projection = glm::perspective(glm::radians(this->fov), this->aspect_ratio, this->near, this->far);
    }
}