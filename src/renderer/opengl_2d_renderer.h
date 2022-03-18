#pragma once

#include "core/common.h"

#include <glm/glm.hpp>
#include <vector>

namespace minecraft {

    struct Quad_Vertex
    {
        glm::vec2 position;
    };

    struct Quad_Instance
    {
        glm::vec2 position;
        glm::vec2 scale;
        glm::vec4 color;
    };

    struct Opengl_2D_Renderer_Data
    {
        u32 quad_vao;

        u32 quad_vbo;
        u32 quad_ibo;

        u32 quad_instance_vbo;

        Quad_Vertex quad_vertices[4];
        u16 quad_indicies[6];

        u32 instance_count_per_patch = 65536;
        std::vector<Quad_Instance> quad_instances;
    };

    struct Opengl_Shader;

    struct Opengl_2D_Renderer
    {
        static Opengl_2D_Renderer_Data internal_data;

        static bool initialize();
        static void shutdown();

        static void begin(f32 ortho_size, f32 aspect_radio, Opengl_Shader *shader);
        static void draw_rect(const glm::vec2& position, const glm::vec2& scale, const glm::vec4& color);
        static void end();
    };
}
