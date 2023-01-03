#include "opengl_2d_renderer.h"
#include "opengl_renderer.h"
#include "opengl_shader.h"
#include "opengl_texture.h"
#include "font.h"

#include <glad/glad.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace minecraft {

    bool Opengl_2D_Renderer::initialize()
    {
        /*
        2----------3
        |          |
        |          |
        |          |
        |          |
        1----------0
        */

        //  1.0f, 1.0f,  0.0f, 1.0f, 1.0f
        // -1.0f, -1.0f, 0.0f, 0.0f, 0.0f
        //  1.0f, -1.0f, 0.0f, 1.0f, 0.0f
        //  1.0f, 1.0f,  0.0f, 1.0f, 1.0f
        // -1.0f, 1.0f,  0.0f, 0.0f, 1.0f
        // -1.0f, -1.0f, 0.0f, 0.0f, 0.0f

        internal_data.quad_vertices[0] = { {  0.5f, -0.5f }, { 1.0f, 0.0f } };
        internal_data.quad_vertices[1] = { { -0.5f, -0.5f }, { 0.0f, 0.0f } };
        internal_data.quad_vertices[2] = { { -0.5f,  0.5f }, { 0.0f, 1.0f } };
        internal_data.quad_vertices[3] = { {  0.5f,  0.5f }, { 1.0f, 1.0f } };

        internal_data.quad_indicies[0] = 3;
        internal_data.quad_indicies[1] = 1;
        internal_data.quad_indicies[2] = 0;

        internal_data.quad_indicies[3] = 3;
        internal_data.quad_indicies[4] = 2;
        internal_data.quad_indicies[5] = 1;

        internal_data.quad_instances.reserve(internal_data.instance_count_per_patch);

        for (u32 i = 0; i < 32; i++)
        {
            internal_data.samplers[i] = i;
            internal_data.texture_slots[i] = -1;
        }

        glGenVertexArrays(1, &internal_data.quad_vao);
        glBindVertexArray(internal_data.quad_vao);

        glGenBuffers(1, &internal_data.quad_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, internal_data.quad_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Quad_Vertex) * 4, internal_data.quad_vertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, false, sizeof(Quad_Vertex), (const void*)offsetof(Quad_Vertex, position));

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(Quad_Vertex), (const void*)offsetof(Quad_Vertex, texture_coords));

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glGenBuffers(1, &internal_data.quad_instance_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, internal_data.quad_instance_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Quad_Instance) * internal_data.instance_count_per_patch, nullptr, GL_STREAM_DRAW);

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, false, sizeof(Quad_Instance), (const void*)offsetof(Quad_Instance, position));
        glVertexAttribDivisor(2, 1);

        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, false, sizeof(Quad_Instance), (const void*)offsetof(Quad_Instance, scale));
        glVertexAttribDivisor(3, 1);

        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 1, GL_FLOAT, false, sizeof(Quad_Instance), (const void*)offsetof(Quad_Instance, rotation));
        glVertexAttribDivisor(4, 1);

        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 4, GL_FLOAT, false, sizeof(Quad_Instance), (const void*)offsetof(Quad_Instance, color));
        glVertexAttribDivisor(5, 1);

        glEnableVertexAttribArray(6);
        glVertexAttribIPointer(6, 1, GL_INT, sizeof(Quad_Instance), (const void*)offsetof(Quad_Instance, texture_index));
        glVertexAttribDivisor(6, 1);

        glEnableVertexAttribArray(7);
        glVertexAttribPointer(7, 2, GL_FLOAT, false, sizeof(Quad_Instance), (const void*)offsetof(Quad_Instance, uv_scale));
        glVertexAttribDivisor(7, 1);

        glEnableVertexAttribArray(8);
        glVertexAttribPointer(8, 2, GL_FLOAT, false, sizeof(Quad_Instance), (const void*)offsetof(Quad_Instance, uv_offset));
        glVertexAttribDivisor(8, 1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glGenBuffers(1, &internal_data.quad_ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, internal_data.quad_ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u16) * 6, internal_data.quad_indicies, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        u32 white_pxiel = 0xffffffff;
        bool success = initialize_texture(&internal_data.white_pixel,
                                          (u8*)(&white_pxiel),
                                          1,
                                          1,
                                          TextureFormat_RGBA,
                                          TextureUsage_UI);
        Assert(success);

        load_shader(&internal_data.ui_shader, "../assets/shaders/quad.glsl");
        return true;
    }

    void Opengl_2D_Renderer::shutdown()
    {
    }

    void Opengl_2D_Renderer::begin()
    {
        glm::vec2 frame_buffer_size = opengl_renderer_get_frame_buffer_size();
        glm::mat4 projection = glm::ortho(0.0f, frame_buffer_size.x, 0.0f, frame_buffer_size.y); // left right bottom top

        Opengl_Shader *shader = &internal_data.ui_shader;
        bind_shader(shader);
        set_uniform_mat4(shader, "u_projection", glm::value_ptr(projection));
        set_uniform_i32_array(shader, "u_textures", internal_data.samplers, 32);
    }

    void Opengl_2D_Renderer::draw_rect(const glm::vec2& position,
                                       const glm::vec2& scale,
                                       f32 rotation,
                                       const glm::vec4& color,
                                       Opengl_Texture *texture,
                                       const glm::vec2& uv_scale,
                                       const glm::vec2& uv_offset)
    {
        i32 texture_index = -1;

        for (u32 i = 5; i < 32; i++)
        {
            if (internal_data.texture_slots[i] == texture->id)
            {
                texture_index = i;
                break;
            }
        }

        if (texture_index == -1)
        {
            for (u32 i = 5; i < 32; i++)
            {
                if (internal_data.texture_slots[i] == -1)
                {
                    bind_texture(texture, i);
                    internal_data.texture_slots[i] = texture->id;
                    texture_index = i;
                    break;
                }
            }
        }

        Assert(texture_index != -1);

        glm::vec2 size = opengl_renderer_get_frame_buffer_size();
        glm::vec2 top_left_position = position;
        top_left_position.y = size.y - position.y;

        Quad_Instance instance = { top_left_position, scale, rotation, color, texture_index, uv_scale, uv_offset };
        internal_data.quad_instances.emplace_back(instance);
    }

    void Opengl_2D_Renderer::draw_string(Bitmap_Font *font,
                                         String8 text,
                                         const glm::vec2& text_size,
                                         const glm::vec2& position,
                                         const glm::vec4& color)
    {
        glm::vec2 half_text_size = text_size * 0.5f;
        glm::vec2 cursor = position;

        for (i32 i = 0; i < text.count; i++)
        {
            stbtt_aligned_quad quad;
            stbtt_GetPackedQuad(font->glyphs,
                                font->atlas.width,
                                font->atlas.height,
                                text.data[i] - ' ',
                                &cursor.x,
                                &cursor.y,
                                &quad,
                                1); // 1 for opengl, 0 for dx3d

            f32 xp = (quad.x1 + quad.x0) / 2.0f;
            f32 yp = (quad.y1 + quad.y0) / 2.0f;

            f32 sx = (quad.x1 - quad.x0);
            f32 sy = (quad.y1 - quad.y0);

            glm::vec2 uv0 = { quad.s0, quad.t1 };
            glm::vec2 uv1 = { quad.s1, quad.t0 };
            glm::vec2 uv_scale = uv1 - uv0;

            draw_rect(glm::vec2(xp - half_text_size.x, yp + half_text_size.y),
                      glm::vec2(sx, sy),
                      0.0f,
                      color,
                      &font->atlas,
                      uv_scale,
                      uv0);
        }
    }

    void Opengl_2D_Renderer::draw_string(Bitmap_Font *font,
                                         const std::string &text,
                                         const glm::vec2& text_size,
                                         const glm::vec2& position,
                                         const glm::vec4& color)
    {
        glm::vec2 half_text_size = text_size * 0.5f;
        glm::vec2 cursor = position;

        for (i32 i = 0; i < text.length(); i++)
        {
            stbtt_aligned_quad quad;
            stbtt_GetPackedQuad(font->glyphs,
                                font->atlas.width,
                                font->atlas.height,
                                text[i] - ' ',
                                &cursor.x,
                                &cursor.y,
                                &quad,
                                1); // 1 for opengl, 0 for dx3d

            f32 xp = (quad.x1 + quad.x0) / 2.0f;
            f32 yp = (quad.y1 + quad.y0) / 2.0f;

            f32 sx = (quad.x1 - quad.x0);
            f32 sy = (quad.y1 - quad.y0);

            glm::vec2 uv0 = { quad.s0, quad.t1 };
            glm::vec2 uv1 = { quad.s1, quad.t0 };
            glm::vec2 uv_scale = uv1 - uv0;

            draw_rect(glm::vec2(xp - half_text_size.x, yp + half_text_size.y),
                      glm::vec2(sx, sy),
                      0.0f,
                      color,
                      &font->atlas,
                      uv_scale,
                      uv0);
        }
    }

    void Opengl_2D_Renderer::end()
    {
        if (internal_data.quad_instances.size())
        {
            glDisable(GL_DEPTH_TEST);

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glBindBuffer(GL_ARRAY_BUFFER, internal_data.quad_instance_vbo);
            glBufferSubData(GL_ARRAY_BUFFER,
                            0,
                            sizeof(Quad_Instance) * internal_data.quad_instances.size(),
                            internal_data.quad_instances.data());

            glBindVertexArray(internal_data.quad_vao);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, internal_data.quad_ibo);
            glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0, internal_data.quad_instances.size());
            internal_data.quad_instances.resize(0);
        }
    }

    Opengl_2D_Renderer_Data Opengl_2D_Renderer::internal_data;
}