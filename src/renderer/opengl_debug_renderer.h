#pragma once

#include "core/common.h"

#include <glm/glm.hpp>

#include <vector> // todo(harlequin): containers

namespace minecraft {

    struct Line_Vertex
    {
        glm::vec3 position;
        glm::vec4 color;
    };

    struct Opengl_Debug_Renderer_Data
    {
        u32 line_vao_id;
        u32 line_vbo_id;

        std::vector<Line_Vertex> line_vertices;
    };

    struct Opengl_Shader;
    struct Camera;

    struct Opengl_Debug_Renderer
    {
        static Opengl_Debug_Renderer_Data internal_data;
        Opengl_Debug_Renderer() = delete;

        static bool initialize();
        static void shutdown();

        static void begin(Camera *camera, Opengl_Shader *shader, f32 line_thickness = 2.0f);

        static void draw_line(const glm::vec3& start,
                              const glm::vec3& end,
                              const glm::vec4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

        static void draw_rect(const glm::vec3& position,
                              const glm::vec2& half_extends,
                              const glm::vec3& normal,
                              const glm::vec4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

        static void draw_rect(const glm::vec3& p0,
                              const glm::vec3& p1,
                              const glm::vec3& p2,
                              const glm::vec3& p3,
                              const glm::vec4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

        static void draw_cube(const glm::vec3& position,
                              const glm::vec3& half_extends,
                              const glm::vec4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

        static void end();
    };
}
