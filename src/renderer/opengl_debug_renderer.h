#pragma once

#include "core/common.h"

#include "game/math.h"

namespace minecraft {

    struct Memory_Arena;
    struct Opengl_Shader;
    struct Camera;

    bool initialize_opengl_debug_renderer(Memory_Arena *arena);
    void shutdown_opengl_debug_renderer();

    void opengl_debug_renderer_push_line(const glm::vec3& start,
                                         const glm::vec3& end,
                                         const glm::vec4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

    void opengl_debug_renderer_push_rect(const glm::vec3& p0,
                                         const glm::vec3& p1,
                                         const glm::vec3& p2,
                                         const glm::vec3& p3,
                                         const glm::vec4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

    void opengl_debug_renderer_push_cube(const glm::vec3& position,
                                         const glm::vec3& half_extends,
                                         const glm::vec4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

    void opengl_debug_renderer_push_aabb(const AABB& aabb,
                                         const glm::vec4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

    void opengl_debug_renderer_draw_lines(Camera *camera,
                                          Opengl_Shader *shader,
                                          f32 line_thickness = 2.0f);
}
