#include "opengl_debug_renderer.h"
#include "opengl_shader.h"
#include "camera.h"
#include "game/game.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#define MC_LINE_COUNT_PER_PATCH 65536

namespace minecraft {

    bool Opengl_Debug_Renderer::initialize()
    {
        auto& [line_vao_id, line_vbo_id, line_vertices] = internal_data;

        glEnable(GL_LINE_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

        glGenVertexArrays(1, &line_vao_id);
        glBindVertexArray(line_vao_id);

        glGenBuffers(1, &line_vbo_id);
        glBindBuffer(GL_ARRAY_BUFFER, line_vbo_id);
        glBufferData(GL_ARRAY_BUFFER,
                     MC_LINE_COUNT_PER_PATCH * 2 * sizeof(Line_Vertex),
                     nullptr,
                     GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0,
                              3,
                              GL_FLOAT,
                              GL_FALSE,
                              sizeof(Line_Vertex),
                              (const void*)offsetof(Line_Vertex, position));

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1,
                              4,
                              GL_FLOAT,
                              GL_FALSE,
                              sizeof(Line_Vertex),
                              (const void*)offsetof(Line_Vertex, color));

        line_vertices.reserve(2 * MC_LINE_COUNT_PER_PATCH);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        return true;
    }

    void Opengl_Debug_Renderer::shutdown()
    {
    }

    void Opengl_Debug_Renderer::begin(Camera *camera, Opengl_Shader *shader, f32 line_thickness)
    {
        auto& [line_vao_id, line_vbo_id, line_vertices] = internal_data;

        shader->use();
        shader->set_uniform_mat4("u_view", glm::value_ptr(camera->view));
        shader->set_uniform_mat4("u_projection", glm::value_ptr(camera->projection));

        glLineWidth(line_thickness);
    }

    void Opengl_Debug_Renderer::draw_line(const glm::vec3& start,
                                          const glm::vec3& end,
                                          const glm::vec4& color)
    {
        auto& [line_vao_id, line_vbo_id, line_vertices] = internal_data;
        Line_Vertex v0 = { start, color };
        Line_Vertex v1 = { end, color };
        line_vertices.emplace_back(v0);
        line_vertices.emplace_back(v1);
    }

    void Opengl_Debug_Renderer::draw_rect(const glm::vec3& p0,
                                          const glm::vec3& p1,
                                          const glm::vec3& p2,
                                          const glm::vec3& p3,
                                          const glm::vec4& color)
    {
        /*
            2 ------- 3
            |      /  |
            |     /   |
            |    /    |
            |   /     |
            |  /      |
            1 ------- 0
        */

        draw_line(p0, p1, color);
        draw_line(p1, p2, color);
        draw_line(p2, p3, color);
        draw_line(p3, p0, color);
    }



    void Opengl_Debug_Renderer::draw_cube(const glm::vec3& position,
                                          const glm::vec3& half_extends,
                                          const glm::vec4& color)
    {
        glm::vec3 p0 = position + glm::vec3(half_extends.x, half_extends.y, half_extends.z);
        glm::vec3 p1 = position + glm::vec3(-half_extends.x, half_extends.y, half_extends.z);
        glm::vec3 p2 = position + glm::vec3(-half_extends.x, half_extends.y, -half_extends.z);
        glm::vec3 p3 = position + glm::vec3(half_extends.x,  half_extends.y, -half_extends.z);
        glm::vec3 p4 = position + glm::vec3(half_extends.x, -half_extends.y, half_extends.z);
        glm::vec3 p5 = position + glm::vec3(-half_extends.x, -half_extends.y, half_extends.z);
        glm::vec3 p6 = position + glm::vec3(-half_extends.x, -half_extends.y, -half_extends.z);
        glm::vec3 p7 = position + glm::vec3(half_extends.x, -half_extends.y, -half_extends.z);

        draw_line(p0, p1, color);
        draw_line(p1, p2, color);
        draw_line(p2, p3, color);
        draw_line(p3, p0, color);

        draw_line(p4, p5, color);
        draw_line(p5, p6, color);
        draw_line(p6, p7, color);
        draw_line(p7, p4, color);

        draw_line(p0, p4, color);
        draw_line(p1, p5, color);
        draw_line(p2, p6, color);
        draw_line(p3, p7, color);
    }

    void Opengl_Debug_Renderer::draw_aabb(const AABB& aabb, const glm::vec4& color)
    {
        glm::vec3 position = (aabb.min + aabb.max) * 0.5f;
        glm::vec3 half_extends = aabb.max - position;
        draw_cube(position, half_extends, color);
    }

    void Opengl_Debug_Renderer::end()
    {
        auto& [line_vao_id,
               line_vbo_id,
               line_vertices] = internal_data;

        if (line_vertices.size())
        {
            glEnable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            glDepthMask(GL_FALSE);
            glDisable(GL_BLEND);

            glBindVertexArray(line_vao_id);
            glBindBuffer(GL_ARRAY_BUFFER, line_vbo_id);

            u32 patch_count = (u32)glm::ceil((f32)line_vertices.size() / (f32)MC_LINE_COUNT_PER_PATCH);

            for (u32 i = 0; i < patch_count; i++)
            {
                u32 offset = i * MC_LINE_COUNT_PER_PATCH;
                u32 count = MC_LINE_COUNT_PER_PATCH;
                u32 remaining = line_vertices.size() - offset;
                if (remaining < count)
                {
                    count = remaining;
                }
                glBufferSubData(GL_ARRAY_BUFFER,
                                0,
                                count * sizeof(Line_Vertex),
                                &line_vertices[offset]);

                glDrawArrays(GL_LINES, 0, count);
            }

            line_vertices.resize(0);
        }
    }

    Opengl_Debug_Renderer_Data Opengl_Debug_Renderer::internal_data;
}