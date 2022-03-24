#include "opengl_2d_renderer.h"
#include "opengl_shader.h"
#include "opengl_texture.h"

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
        glVertexAttribPointer(4, 4, GL_FLOAT, false, sizeof(Quad_Instance), (const void*)offsetof(Quad_Instance, color));
        glVertexAttribDivisor(4, 1);

        glEnableVertexAttribArray(5);
        glVertexAttribIPointer(5, 1, GL_INT, sizeof(Quad_Instance), (const void*)offsetof(Quad_Instance, texture_index));
        glVertexAttribDivisor(5, 1);

        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 2, GL_FLOAT, false, sizeof(Quad_Instance), (const void*)offsetof(Quad_Instance, uv_scale));
        glVertexAttribDivisor(6, 1);

        glEnableVertexAttribArray(7);
        glVertexAttribPointer(7, 2, GL_FLOAT, false, sizeof(Quad_Instance), (const void*)offsetof(Quad_Instance, uv_offset));
        glVertexAttribDivisor(7, 1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glGenBuffers(1, &internal_data.quad_ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, internal_data.quad_ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u16) * 6, internal_data.quad_indicies, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        return true;
    }

    void Opengl_2D_Renderer::shutdown()
    {
    }

    void Opengl_2D_Renderer::begin(f32 ortho_size, f32 aspect_ratio, Opengl_Shader *shader)
    {
        shader->use();

        f32 half_ortho_size = ortho_size / 2.0f;
        glm::vec3 camera_position = glm::vec3(0.0f, 0.0f, 0.0f);
        glm::mat4 view = glm::translate(glm::mat4(1.0f), -camera_position);
        glm::mat4 projection = glm::ortho(-half_ortho_size * aspect_ratio, half_ortho_size * aspect_ratio, -half_ortho_size, half_ortho_size);
        shader->set_uniform_mat4("u_view", glm::value_ptr(view));
        shader->set_uniform_mat4("u_projection", glm::value_ptr(projection));
        shader->set_uniform_i32_array("u_textures", internal_data.samplers, 32);
    }

    void Opengl_2D_Renderer::draw_rect(const glm::vec2& position,
                                       const glm::vec2& scale,
                                       const glm::vec4& color,
                                       Opengl_Texture *texture,
                                       const glm::vec2& uv_scale,
                                       const glm::vec2& uv_offset)
    {
        i32 texture_index = -1;

        for (u32 i = 2; i < 32; i++)
        {
            if (internal_data.texture_slots[i] == texture->id)
            {
                texture_index = i;
                break;
            }
        }

        if (texture_index == -1)
        {
            for (u32 i = 2; i < 32; i++)
            {
                if (internal_data.texture_slots[i] == -1)
                {
                    texture->bind(i);
                    internal_data.texture_slots[i] = texture->id;
                    texture_index = i;
                    break;
                }
            }
        }

        assert(texture_index != -1);
        Quad_Instance instance = { position, scale, color, texture_index, uv_scale, uv_offset };
        internal_data.quad_instances.emplace_back(instance);
    }

    void Opengl_2D_Renderer::end()
    {
        if (internal_data.quad_instances.size())
        {
            // alpha blending
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

            glDisable(GL_BLEND);
        }
    }

    Opengl_2D_Renderer_Data Opengl_2D_Renderer::internal_data;
}