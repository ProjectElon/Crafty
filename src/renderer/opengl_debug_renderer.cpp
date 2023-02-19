#include "opengl_debug_renderer.h"
#include "opengl_shader.h"
#include "opengl_vertex_array.h"
#include "camera.h"
#include "game/game.h"
#include "memory/memory_arena.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#define MAX_LINE_COUNT 65536

namespace minecraft {

    // todo(harlequin): right now we are using a color per line vertex
    // should we switch to using a color per line instance ?
    struct Line_Vertex
    {
        glm::vec3 position;
        glm::vec4 color;
    };

    struct Opengl_Debug_Renderer
    {
        Opengl_Vertex_Array line_vertex_array;

        u32          line_count;
        Line_Vertex *line_vertices;
    };

    static Opengl_Debug_Renderer *debug_renderer;

    bool initialize_opengl_debug_renderer(Memory_Arena *arena)
    {
        if (debug_renderer)
        {
            return false;
        }

        debug_renderer = ArenaPushAlignedZero(arena, Opengl_Debug_Renderer);
        Assert(debug_renderer);

        glEnable(GL_LINE_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

        GLbitfield flags = GL_MAP_PERSISTENT_BIT |
                           GL_MAP_WRITE_BIT |
                           GL_MAP_COHERENT_BIT;

        Opengl_Vertex_Array line_vertex_array   = begin_vertex_array(arena);
        Opengl_Vertex_Buffer line_vertex_buffer = push_vertex_buffer(&line_vertex_array,
                                                                     sizeof(Line_Vertex),
                                                                     2 * MAX_LINE_COUNT,
                                                                     nullptr,
                                                                     flags);
        push_vertex_attribute(&line_vertex_array,
                              &line_vertex_buffer,
                              "position",
                              VertexAttributeType_V3,
                              offsetof(Line_Vertex, position));

        push_vertex_attribute(&line_vertex_array,
                              &line_vertex_buffer,
                              "color",
                              VertexAttributeType_V4,
                              offsetof(Line_Vertex, color));

        end_vertex_array(&line_vertex_array);
        debug_renderer->line_vertex_array = line_vertex_array;
        debug_renderer->line_vertices = (Line_Vertex*)line_vertex_buffer.data;

        return true;
    }

    void shutdown_opengl_debug_renderer()
    {
    }

    void opengl_debug_renderer_draw_lines(Camera *camera,
                                          Opengl_Shader *shader,
                                          f32 line_thickness /* = 2.0f */)
    {
        if (!debug_renderer->line_count)
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

        bind_vertex_array(&debug_renderer->line_vertex_array);

        glDrawArrays(GL_LINES, 0, 2 * debug_renderer->line_count);
        debug_renderer->line_count = 0;
    }

    void opengl_debug_renderer_push_line(const glm::vec3& start,
                                         const glm::vec3& end,
                                         const glm::vec4& color)
    {
        Assert(debug_renderer->line_count < MAX_LINE_COUNT);
        debug_renderer->line_vertices[(debug_renderer->line_count * 2) + 0] = { start, color };
        debug_renderer->line_vertices[(debug_renderer->line_count * 2) + 1] = { end,   color };
        u32 current_line_vertex_index = debug_renderer->line_count++;
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