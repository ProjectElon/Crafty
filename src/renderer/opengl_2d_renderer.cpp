#include "opengl_2d_renderer.h"
#include "opengl_shader.h"

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
        internal_data.quad_vertices[0] = { {  0.5f, -0.5f } };
        internal_data.quad_vertices[1] = { { -0.5f, -0.5f } };
        internal_data.quad_vertices[2] = { { -0.5f,  0.5f } };
        internal_data.quad_vertices[3] = { {  0.5f,  0.5f } };

        internal_data.quad_indicies[0] = 3;
        internal_data.quad_indicies[1] = 1;
        internal_data.quad_indicies[2] = 0;

        internal_data.quad_indicies[3] = 3;
        internal_data.quad_indicies[4] = 2;
        internal_data.quad_indicies[5] = 1;

        internal_data.quad_instances.reserve(internal_data.instance_count_per_patch);

        glGenVertexArrays(1, &internal_data.quad_vao);
        glBindVertexArray(internal_data.quad_vao);

        glGenBuffers(1, &internal_data.quad_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, internal_data.quad_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Quad_Vertex) * 4, internal_data.quad_vertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, false, sizeof(Quad_Vertex), (const void*)offsetof(Quad_Vertex, position));

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glGenBuffers(1, &internal_data.quad_instance_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, internal_data.quad_instance_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Quad_Instance) * internal_data.instance_count_per_patch, nullptr, GL_STREAM_DRAW);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(Quad_Instance), (const void*)offsetof(Quad_Instance, position));
        glVertexAttribDivisor(1, 1);

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, false, sizeof(Quad_Instance), (const void*)offsetof(Quad_Instance, scale));
        glVertexAttribDivisor(2, 1);

        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, false, sizeof(Quad_Instance), (const void*)offsetof(Quad_Instance, color));
        glVertexAttribDivisor(3, 1);

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
    }

    void Opengl_2D_Renderer::draw_rect(const glm::vec2& position, const glm::vec2& scale, const glm::vec4& color)
    {
        Quad_Instance instance = { position, scale, color };
        internal_data.quad_instances.emplace_back(instance);
    }

    void Opengl_2D_Renderer::end()
    {
        if (internal_data.quad_instances.size())
        {
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