#pragma once

#include "core/common.h"
#include "containers/string.h"

#include <glm/glm.hpp>

#include <vector>

namespace minecraft {

    #define MAX_QUAD_COUNT 65536

    struct Opengl_Shader;
    struct Opengl_Texture;
    struct Opengl_Texture_Atlas;
    struct Bitmap_Font;
    struct Memory_Arena;

    bool initialize_opengl_2d_renderer(Memory_Arena *arena);
    void shutdown_opengl_2d_renderer();

    void opengl_2d_renderer_push_quad(const glm::vec2 &position,
                                      const glm::vec2 &scale,
                                      f32              rotation,
                                      const glm::vec4 &color,
                                      Opengl_Texture  *texture    = nullptr,
                                      const glm::vec2 &uv_scale  = { 1.0f, 1.0f },
                                      const glm::vec2 &uv_offset = { 0.0f, 0.0f });

    void opengl_2d_renderer_push_quad(const glm::vec2      &position,
                                      const glm::vec2      &scale,
                                      f32                   rotation,
                                      const glm::vec4      &color,
                                      Opengl_Texture_Atlas *atlas,
                                      u32                   sub_texture_index);

    void opengl_2d_renderer_push_string(Bitmap_Font *font,
                                        String8 text,
                                        const glm::vec2& text_size,
                                        const glm::vec2& position,
                                        const glm::vec4& color);

    void opengl_2d_renderer_draw_quads(Opengl_Shader *shader);
}
