#include "opengl_2d_renderer.h"
#include "opengl_renderer.h"
#include "opengl_shader.h"
#include "opengl_texture.h"
#include "opengl_texture_atlas.h"
#include "opengl_vertex_array.h"
#include "font.h"
#include "memory/memory_arena.h"

#include <glad/glad.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace minecraft {

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
        Opengl_Vertex_Array quad_vertex_array;

        i32 samplers[32];
        i32 texture_slots[32];

        u32            quad_count;
        Quad_Instance *quad_instances;

        Opengl_Texture white_pixel;
    };

    static Opengl_2D_Renderer *renderer_2d;

    bool initialize_opengl_2d_renderer(Memory_Arena *arena)
    {
        if (renderer_2d)
        {
            return false;
        }

        renderer_2d = ArenaPushAlignedZero(arena, Opengl_2D_Renderer);
        Assert(renderer_2d);

        for (u32 i = 0; i < 32; i++)
        {
            renderer_2d->samplers[i]      = i;
            renderer_2d->texture_slots[i] = -1;
        }

        Opengl_Vertex_Array quad_vertex_array = begin_vertex_array(arena);
        GLbitfield flags = GL_MAP_PERSISTENT_BIT |
                           GL_MAP_WRITE_BIT |
                           GL_MAP_COHERENT_BIT;

        Opengl_Vertex_Buffer quad_vertex_buffer = push_vertex_buffer(&quad_vertex_array,
                           sizeof(Quad_Instance),
                           MAX_QUAD_COUNT,
                           nullptr,
                           flags);

        bool per_instance = true;
        push_vertex_attribute(&quad_vertex_array, &quad_vertex_buffer, "position", VertexAttributeType_V2, offsetof(Quad_Instance, position), per_instance);

        push_vertex_attribute(&quad_vertex_array, &quad_vertex_buffer, "scale", VertexAttributeType_V2, offsetof(Quad_Instance, scale), per_instance);

        push_vertex_attribute(&quad_vertex_array, &quad_vertex_buffer, "rotation", VertexAttributeType_F32, offsetof(Quad_Instance, rotation), per_instance);

        push_vertex_attribute(&quad_vertex_array, &quad_vertex_buffer, "color", VertexAttributeType_V4, offsetof(Quad_Instance, color), per_instance);

        push_vertex_attribute(&quad_vertex_array, &quad_vertex_buffer, "texture_index", VertexAttributeType_I32, offsetof(Quad_Instance, texture_index), per_instance);

        push_vertex_attribute(&quad_vertex_array, &quad_vertex_buffer, "uv_scale", VertexAttributeType_V2, offsetof(Quad_Instance, uv_scale), per_instance);

        push_vertex_attribute(&quad_vertex_array, &quad_vertex_buffer, "uv_offset", VertexAttributeType_V2, offsetof(Quad_Instance, uv_offset), per_instance);

        end_vertex_array(&quad_vertex_array);
        renderer_2d->quad_vertex_array = quad_vertex_array;
        renderer_2d->quad_instances    = (Quad_Instance*)quad_vertex_buffer.data;

        // todo(harlequin): default assets concept
        u32 white_pxiel = 0xffffffff;
        bool success = initialize_texture(&renderer_2d->white_pixel,
                                          (u8*)(&white_pxiel),
                                          1,
                                          1,
                                          TextureFormat_RGBA8,
                                          TextureUsage_UI);
        Assert(success);

        return true;
    }

    void shutdown_opengl_2d_renderer()
    {
    }

    void opengl_2d_renderer_draw_quads(Opengl_Shader *shader)
    {
        if (!renderer_2d->quad_count)
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
        set_uniform_i32_array(shader, "u_textures", renderer_2d->samplers, ArrayCount(renderer_2d->samplers));

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        bind_vertex_array(&renderer_2d->quad_vertex_array);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, renderer_2d->quad_count);
        renderer_2d->quad_count = 0;
    }

    void opengl_2d_renderer_push_quad(const glm::vec2& position,
                                      const glm::vec2& scale,
                                      f32 rotation,
                                      const glm::vec4& color,
                                      Opengl_Texture *texture,
                                      const glm::vec2& uv_scale,
                                      const glm::vec2& uv_offset)
    {
        Assert(renderer_2d->quad_count < MAX_QUAD_COUNT);

        if (!texture)
        {
            texture = &renderer_2d->white_pixel;
        }

        i32 texture_index = -1;

        for (u32 i = 5; i < 32; i++)
        {
            if (renderer_2d->texture_slots[i] == texture->handle)
            {
                texture_index = i;
                break;
            }
        }

        if (texture_index == -1)
        {
            for (u32 i = 5; i < 32; i++)
            {
                if (renderer_2d->texture_slots[i] == -1)
                {
                    bind_texture(texture, i);
                    renderer_2d->texture_slots[i] = texture->handle;
                    texture_index = i;
                    break;
                }
            }
        }

        Assert(texture_index != -1);

        glm::vec2 size              = opengl_renderer_get_frame_buffer_size();
        glm::vec2 top_left_position = position;
        top_left_position.y         = size.y - position.y;

        Quad_Instance *quad = &renderer_2d->quad_instances[renderer_2d->quad_count++];
        quad->position      = top_left_position;
        quad->scale         = scale;
        quad->rotation      = rotation;
        quad->color         = color;
        quad->texture_index = texture_index;
        quad->uv_scale      = uv_scale;
        quad->uv_offset     = uv_offset;
    }

    void opengl_2d_renderer_push_quad(const glm::vec2      &position,
                                      const glm::vec2      &scale,
                                      f32                   rotation,
                                      const glm::vec4      &color,
                                      Opengl_Texture_Atlas *atlas,
                                      u32                   sub_texture_index)
    {
        const Texture_Coords *coords = get_sub_texture_coords(atlas, sub_texture_index);
        opengl_2d_renderer_push_quad(position,
                                     scale,
                                     rotation,
                                     color,
                                     atlas->texture,
                                     coords->scale,
                                     coords->offset);
    }

    void opengl_2d_renderer_push_string(Bitmap_Font     *font,
                                        const String8   &text,
                                        const glm::vec2 &text_size,
                                        const glm::vec2 &position,
                                        const glm::vec4 &color)
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