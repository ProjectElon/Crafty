#include "opengl_debug_renderer.h"
#include "opengl_shader.h"
#include "camera.h"
#include "game/game.h"
#include "memory/memory_arena.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#define MAX_LINE_COUNT 65536

namespace minecraft {

    struct Line_Vertex
    {
        glm::vec3 position;
        glm::vec4 color;
    };

    struct Opengl_Debug_Renderer
    {
        u32 line_vao_id;
        u32 line_vbo_id;

        u32         line_count;
        Line_Vertex line_vertices[2 * MAX_LINE_COUNT];
    };

    static Opengl_Debug_Renderer *renderer;

    bool initialize_opengl_debug_renderer(Memory_Arena *arena)
    {
        if (renderer)
        {
            return false;
        }

        renderer = ArenaPushAlignedZero(arena, Opengl_Debug_Renderer);

        glEnable(GL_LINE_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

        glGenVertexArrays(1, &renderer->line_vao_id);
        glBindVertexArray(renderer->line_vao_id);

        glGenBuffers(1, &renderer->line_vbo_id);
        glBindBuffer(GL_ARRAY_BUFFER, renderer->line_vbo_id);
        glBufferData(GL_ARRAY_BUFFER,
                     MAX_LINE_COUNT * 2 * sizeof(Line_Vertex),
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

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        return true;
    }

    void shutdown_opengl_debug_renderer()
    {
    }

    void opengl_debug_renderer_draw_lines(Camera *camera,
                                          Opengl_Shader *shader,
                                          f32 line_thickness /* = 2.0f */)
    {
        if (!renderer->line_count)
        {
            return;
        }

        bind_shader(shader);
        set_uniform_mat4(shader, "u_view", glm::value_ptr(camera->view));
        set_uniform_mat4(shader, "u_projection", glm::value_ptr(camera->projection));

        glDisable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glLineWidth(line_thickness);

        glBindVertexArray(renderer->line_vao_id);
        glBindBuffer(GL_ARRAY_BUFFER, renderer->line_vbo_id);

        glBufferSubData(GL_ARRAY_BUFFER,
                        0,
                        2 * renderer->line_count * sizeof(Line_Vertex),
                        renderer->line_vertices);

        glDrawArrays(GL_LINES, 0, 2 * renderer->line_count);
        renderer->line_count = 0;
    }

    void opengl_debug_renderer_push_line(const glm::vec3& start,
                                         const glm::vec3& end,
                                         const glm::vec4& color)
    {
        Assert(renderer->line_count < MAX_LINE_COUNT);
        u32 current_line_vertex_index = renderer->line_count++ * 2;
        renderer->line_vertices[current_line_vertex_index + 0] = { start, color };
        renderer->line_vertices[current_line_vertex_index + 1] = { end,   color };
    }

    void opengl_debug_renderer_push_rect(const glm::vec3& p0,
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

        opengl_debug_renderer_push_line(p0, p1, color);
        opengl_debug_renderer_push_line(p1, p2, color);
        opengl_debug_renderer_push_line(p2, p3, color);
        opengl_debug_renderer_push_line(p3, p0, color);
    }



    void opengl_debug_renderer_push_cube(const glm::vec3& position,
                                         const glm::vec3& half_extends,
                                         const glm::vec4& color)
    {
        glm::vec3 p0 = position + glm::vec3(half_extends.x,   half_extends.y,  half_extends.z);
        glm::vec3 p1 = position + glm::vec3(-half_extends.x,  half_extends.y,  half_extends.z);
        glm::vec3 p2 = position + glm::vec3(-half_extends.x,  half_extends.y, -half_extends.z);
        glm::vec3 p3 = position + glm::vec3(half_extends.x,   half_extends.y, -half_extends.z);
        glm::vec3 p4 = position + glm::vec3(half_extends.x,  -half_extends.y,  half_extends.z);
        glm::vec3 p5 = position + glm::vec3(-half_extends.x, -half_extends.y,  half_extends.z);
        glm::vec3 p6 = position + glm::vec3(-half_extends.x, -half_extends.y, -half_extends.z);
        glm::vec3 p7 = position + glm::vec3(half_extends.x,  -half_extends.y, -half_extends.z);

        opengl_debug_renderer_push_line(p0, p1, color);
        opengl_debug_renderer_push_line(p1, p2, color);
        opengl_debug_renderer_push_line(p2, p3, color);
        opengl_debug_renderer_push_line(p3, p0, color);

        opengl_debug_renderer_push_line(p4, p5, color);
        opengl_debug_renderer_push_line(p5, p6, color);
        opengl_debug_renderer_push_line(p6, p7, color);
        opengl_debug_renderer_push_line(p7, p4, color);

        opengl_debug_renderer_push_line(p0, p4, color);
        opengl_debug_renderer_push_line(p1, p5, color);
        opengl_debug_renderer_push_line(p2, p6, color);
        opengl_debug_renderer_push_line(p3, p7, color);
    }

    void opengl_debug_renderer_push_aabb(const AABB& aabb, const glm::vec4& color)
    {
        glm::vec3 position     = (aabb.min + aabb.max) * 0.5f;
        glm::vec3 half_extends = aabb.max - position;
        opengl_debug_renderer_push_cube(position, half_extends, color);
    }
}