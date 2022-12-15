#pragma once

#include "core/common.h"
#include "containers/string.h"

#include "renderer/opengl_texture.h"
#include "renderer/opengl_shader.h"

#include <glm/glm.hpp>

#include <vector>

namespace minecraft {

    struct Quad_Vertex
    {
        glm::vec2 position;
        glm::vec2 texture_coords;
    };

    struct Quad_Instance
    {
        glm::vec2 position;
        glm::vec2 scale;
        f32 rotation;
        glm::vec4 color;
        i32 texture_index;
        glm::vec2 uv_scale;
        glm::vec2 uv_offset;
    };

    struct Opengl_2D_Renderer_Data
    {
        u32 quad_vao;

        u32 quad_vbo;
        u32 quad_ibo;

        u32 quad_instance_vbo;

        Quad_Vertex quad_vertices[4];
        u16 quad_indicies[6];

        i32 samplers[32];
        i32 texture_slots[32];

        u32 instance_count_per_patch = 65536;
        std::vector<Quad_Instance> quad_instances;

        Opengl_Texture white_pixel;
        Opengl_Shader  ui_shader;
    };

    struct Bitmap_Font;

    struct Opengl_2D_Renderer
    {
        static Opengl_2D_Renderer_Data internal_data;

        static bool initialize();
        static void shutdown();

        static void begin();

        static void draw_rect(const glm::vec2& position,
                              const glm::vec2& scale,
                              f32 rotation,
                              const glm::vec4& color,
                              Opengl_Texture *texture = &internal_data.white_pixel,
                              const glm::vec2& uv_scale  = { 1.0f, 1.0f },
                              const glm::vec2& uv_offset = { 0.0f, 0.0f });

        static void draw_string(Bitmap_Font *font,
                                String8 text,
                                const glm::vec2& text_size,
                                const glm::vec2& position,
                                const glm::vec4& color);

        // @Temprary
        static void draw_string(Bitmap_Font *font,
                                const std::string& text,
                                const glm::vec2& text_size,
                                const glm::vec2& position,
                                const glm::vec4& color);


        static void end();
    };
}
