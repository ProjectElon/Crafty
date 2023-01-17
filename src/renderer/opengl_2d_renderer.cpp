#include "opengl_2d_renderer.h"
#include "opengl_renderer.h"
#include "opengl_shader.h"
#include "opengl_texture.h"
#include "font.h"
#include "memory/memory_arena.h"

#include <glad/glad.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace minecraft {

    struct Quad_Vertex
    {
        glm::vec2 position;
        glm::vec2 texture_coords;
    };

    struct Quad_Instance
    {
        glm::vec2  position;
        glm::vec2  scale;
        f32        rotation;
        glm::vec4  color;
        i32        texture_index;
        glm::vec2  uv_scale;
        glm::vec2  uv_offset;
    };

    struct Opengl_2D_Renderer
    {
        u32 quad_vao;

        u32 quad_vbo;
        u32 quad_ibo;

        u32 quad_instance_vbo;

        Quad_Vertex quad_vertices[4];
        u16         quad_indicies[6];

        i32 samplers[32];
        i32 texture_slots[32];

        u32           quad_count;
        Quad_Instance quad_instances[MAX_QUAD_COUNT];

        Opengl_Texture  white_pixel;
    };

    static Opengl_2D_Renderer *renderer;

    bool initialize_opengl_2d_renderer(Memory_Arena *arena)
    {
        if (renderer)
        {
            return false;
        }

        renderer = ArenaPushAlignedZero(arena, Opengl_2D_Renderer);

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

        renderer->quad_vertices[0] = { {  0.5f, -0.5f }, { 1.0f, 0.0f } };
        renderer->quad_vertices[1] = { { -0.5f, -0.5f }, { 0.0f, 0.0f } };
        renderer->quad_vertices[2] = { { -0.5f,  0.5f }, { 0.0f, 1.0f } };
        renderer->quad_vertices[3] = { {  0.5f,  0.5f }, { 1.0f, 1.0f } };

        renderer->quad_indicies[0] = 3;
        renderer->quad_indicies[1] = 1;
        renderer->quad_indicies[2] = 0;

        renderer->quad_indicies[3] = 3;
        renderer->quad_indicies[4] = 2;
        renderer->quad_indicies[5] = 1;

        for (u32 i = 0; i < 32; i++)
        {
            renderer->samplers[i]      = i;
            renderer->texture_slots[i] = -1;
        }

        glGenVertexArrays(1, &renderer->quad_vao);
        glBindVertexArray(renderer->quad_vao);

        glGenBuffers(1, &renderer->quad_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, renderer->quad_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Quad_Vertex) * 4, renderer->quad_vertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, false, sizeof(Quad_Vertex), (const void*)offsetof(Quad_Vertex, position));

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(Quad_Vertex), (const void*)offsetof(Quad_Vertex, texture_coords));

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glGenBuffers(1, &renderer->quad_instance_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, renderer->quad_instance_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Quad_Instance) * MAX_QUAD_COUNT, nullptr, GL_STREAM_DRAW);

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

        glGenBuffers(1, &renderer->quad_ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->quad_ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u16) * 6, renderer->quad_indicies, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        u32 white_pxiel = 0xffffffff;
        bool success = initialize_texture(&renderer->white_pixel,
                                          (u8*)(&white_pxiel),
                                          1,
                                          1,
                                          TextureFormat_RGBA,
                                          TextureUsage_UI);
        Assert(success);

        return true;
    }

    void shutdown_opengl_2d_renderer()
    {
    }

    void opengl_2d_renderer_draw_quads(Opengl_Shader *shader)
    {
        if (!renderer->quad_count)
        {
            return;
        }

        Assert(shader);

        glm::vec2 frame_buffer_size = opengl_renderer_get_frame_buffer_size();

        f32 left   = 0.0f;
        f32 right  = frame_buffer_size.x;
        f32 bottom = 0.0f;
        f32 top    = frame_buffer_size.y;
        glm::mat4 projection  = glm::ortho(left, right, bottom, top);

        bind_shader(shader);
        set_uniform_mat4(shader, "u_projection", glm::value_ptr(projection));
        set_uniform_i32_array(shader, "u_textures", renderer->samplers, 32);

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glBindBuffer(GL_ARRAY_BUFFER, renderer->quad_instance_vbo);
        glBufferSubData(GL_ARRAY_BUFFER,
                        0,
                        sizeof(Quad_Instance) * renderer->quad_count,
                        renderer->quad_instances);

        glBindVertexArray(renderer->quad_vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->quad_ibo);
        glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0, renderer->quad_count);
        renderer->quad_count = 0;
    }

    void opengl_2d_renderer_push_quad(const glm::vec2& position,
                                      const glm::vec2& scale,
                                      f32 rotation,
                                      const glm::vec4& color,
                                      Opengl_Texture *texture,
                                      const glm::vec2& uv_scale,
                                      const glm::vec2& uv_offset)
    {
        Assert(renderer->quad_count < MAX_QUAD_COUNT);

        if (!texture)
        {
            texture = &renderer->white_pixel;
        }

        i32 texture_index = -1;

        for (u32 i = 5; i < 32; i++)
        {
            if (renderer->texture_slots[i] == texture->id)
            {
                texture_index = i;
                break;
            }
        }

        if (texture_index == -1)
        {
            for (u32 i = 5; i < 32; i++)
            {
                if (renderer->texture_slots[i] == -1)
                {
                    bind_texture(texture, i);
                    renderer->texture_slots[i] = texture->id;
                    texture_index = i;
                    break;
                }
            }
        }

        Assert(texture_index != -1);

        glm::vec2 size              = opengl_renderer_get_frame_buffer_size();
        glm::vec2 top_left_position = position;
        top_left_position.y         = size.y - position.y;

        renderer->quad_instances[renderer->quad_count++] = { top_left_position, scale, rotation, color, texture_index, uv_scale, uv_offset };
    }

    void opengl_2d_renderer_push_string(Bitmap_Font *font,
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

            opengl_2d_renderer_push_quad(glm::vec2(xp - half_text_size.x, yp + half_text_size.y),
                                         glm::vec2(sx, sy),
                                         0.0f,
                                         color,
                                         &font->atlas,
                                         uv_scale,
                                         uv0);
        }
    }
}