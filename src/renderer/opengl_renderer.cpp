#include "opengl_renderer.h"
#include "core/platform.h"
#include "camera.h"
#include "opengl_shader.h"
#include "opengl_debug_renderer.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include "meta/spritesheet_meta.h"

namespace minecraft {

#define BLOCK_X_MASK 15 // 4 bits
#define BLOCK_Y_MASK 255 // 8 bits
#define BLOCK_Z_MASK 15 // 4 bits
#define LOCAL_POSITION_ID_MASK 7 // 3 bits
#define FACE_ID_MASK 7 // 3 bits
#define FACE_CORNER_ID_MASK 3 // 2 bits

    static void APIENTRY gl_debug_output(GLenum source,
                                         GLenum type,
                                         u32 id,
                                         GLenum severity,
                                         GLsizei length,
                                         const char *message,
                                         const void *user_param);

    bool Opengl_Renderer::initialize(Platform *platform)
    {
        if (!platform->opengl_initialize())
        {
            fprintf(stderr, "[ERROR]: failed to initialize opengl\n");
            return false;
        }

#ifndef MC_DIST
        i32 flags;
        glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
        if (flags & GL_CONTEXT_FLAG_DEBUG_BIT)
        {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(gl_debug_output, &internal_data);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        }
#endif
        // depth testing
        glEnable(GL_DEPTH_TEST);

        // alpha blending
        // glEnable(GL_BLEND);
	// glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // multisampling
        glEnable(GL_MULTISAMPLE);

        // face culling
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        // fireframe mode
        // glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

        internal_data.platform = platform;
        const char *block_sprite_sheet_path = "../assets/textures/spritesheet.png"; // todo(Harlequin): rename spritesheet to block_spritesheet
        bool success = internal_data.block_sprite_sheet.load_from_file(block_sprite_sheet_path);

        if (!success)
        {
            fprintf(stderr, "[ERROR]: failed to load texture at %s\n", block_sprite_sheet_path);
            return false;
        }

        glGenBuffers(1, &internal_data.uv_buffer_id);
        glBindBuffer(GL_TEXTURE_BUFFER, internal_data.uv_buffer_id);
        glBufferData(GL_TEXTURE_BUFFER, MC_PACKED_TEXTURE_COUNT * 8 * sizeof(f32), texture_uv_rects, GL_STATIC_DRAW);

        glGenTextures(1, &internal_data.uv_texture_id);
        glBindTexture(GL_TEXTURE_BUFFER, internal_data.uv_texture_id);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, internal_data.uv_buffer_id);

        glBindBuffer(GL_TEXTURE_BUFFER, 0);
        glBindTexture(GL_TEXTURE_BUFFER, 0);

        // todo(harlequin): memory system
        u32 *chunk_indicies = new u32[MC_INDEX_COUNT_PER_CHUNK];
        defer { delete[] chunk_indicies; };

        u32 element_index = 0;
        u32 vertex_index = 0;

        for (u32 i = 0; i < MC_BLOCK_COUNT_PER_CHUNK * 6; i++)
        {
            chunk_indicies[element_index + 0] = vertex_index + 3;
            chunk_indicies[element_index + 1] = vertex_index + 1;
            chunk_indicies[element_index + 2] = vertex_index + 0;

            chunk_indicies[element_index + 3] = vertex_index + 3;
            chunk_indicies[element_index + 4] = vertex_index + 2;
            chunk_indicies[element_index + 5] = vertex_index + 1;

            element_index += 6;
            vertex_index += 4;
        }

        glGenBuffers(1, &internal_data.chunk_index_buffer_id);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, internal_data.chunk_index_buffer_id);

        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            MC_INDEX_COUNT_PER_CHUNK * sizeof(u32),
            chunk_indicies,
            GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        return true;
    }

    void Opengl_Renderer::shutdown()
    {
    }

    bool Opengl_Renderer::on_resize(const Event* event, void *sender)
    {
        u32 width;
        u32 height;
        Event_System::parse_resize_event(event, &width, &height);
        glViewport(0, 0, width, height);
        return false;
    }

    u32 Opengl_Renderer::compress_vertex(const glm::ivec3& block_coords,
                               u32 local_position_id,
                               u32 face_id,
                               u32 face_corner_id,
                               u32 flags)
    {
        u32 result = 0;

        result |= block_coords.x;
        result |= ((u32)block_coords.y << 4);
        result |= ((u32)block_coords.z << 12);
        result |= ((u32)local_position_id << 16);
        result |= ((u32)face_id << 19);
        result |= ((u32)face_corner_id << 22);
        result |= ((u32)flags << 24);

        return result;
    }

    void Opengl_Renderer::extract_vertex(u32 vertex,
        glm::ivec3& block_coords,
        u32 &out_local_position_id,
        u32 &out_face_id,
        u32 &out_face_corner_id,
        u32 &out_flags)
    {
        block_coords.x = vertex & BLOCK_X_MASK;
        block_coords.y = (vertex >> 4) & BLOCK_Y_MASK;
        block_coords.z = (vertex >> 12) & BLOCK_Z_MASK;
        out_local_position_id = (vertex >> 16) & LOCAL_POSITION_ID_MASK;
        out_face_id = (vertex >> 19) & FACE_ID_MASK;
        out_face_corner_id = (vertex >> 22) & FACE_CORNER_ID_MASK;
        out_flags = (vertex >> 24);
    }

    static void submit_block_face(
        const glm::ivec3& block_coords,
        u32 uv_rect_id,
        BlockFaceId face,
        u32 p0,
        u32 p1,
        u32 p2,
        u32 p3,
        Chunk *chunk,
        Block* block,
        Block* block_facing_normal)
    {
        const Block_Info& block_facing_normal_info = World::block_infos[block_facing_normal->id];

        if (!block_facing_normal_info.is_solid() || block_facing_normal_info.is_transparent())
        {
            const Block_Info& block_info = World::block_infos[block->id];
            const u32& block_flags = block_info.flags;

            Chunk_Render_Data& chunk_render_data = chunk->render_data;
            Vertex *vertices = chunk_render_data.vertices;
            u32 vertex_index = chunk_render_data.vertex_count;

            u32 data00 = Opengl_Renderer::compress_vertex(block_coords, p0, face, BlockFaceCornerId_BottomRight, block_flags);
            u32 data01 = Opengl_Renderer::compress_vertex(block_coords, p1, face, BlockFaceCornerId_BottomLeft,  block_flags);
            u32 data02 = Opengl_Renderer::compress_vertex(block_coords, p2, face, BlockFaceCornerId_TopLeft,     block_flags);
            u32 data03 = Opengl_Renderer::compress_vertex(block_coords, p3, face, BlockFaceCornerId_TopRight,    block_flags);

            vertices[vertex_index + 0] = { data00, uv_rect_id * 8 + BlockFaceCornerId_BottomRight * 2 };
            vertices[vertex_index + 1] = { data01, uv_rect_id * 8 + BlockFaceCornerId_BottomLeft  * 2 };
            vertices[vertex_index + 2] = { data02, uv_rect_id * 8 + BlockFaceCornerId_TopLeft     * 2 };
            vertices[vertex_index + 3] = { data03, uv_rect_id * 8 + BlockFaceCornerId_TopRight    * 2 };

            chunk_render_data.vertex_count += 4;
            chunk_render_data.face_count++;
        }
    }

    static void submit_block(
        Chunk *chunk,
        Block *block,
        const glm::ivec3 &block_coords)
    {
        const Block_Info& block_info = World::block_infos[block->id];

         /*
          1----------2
          |\         |\
          | 0--------|-3
          | |        | |
          5-|--------6 |
           \|         \|
            4----------7
        */

        /*
            top face

             2 ----- 3
            |      /  |
            |     /   |
            |    /    |
            |   /     |
            |  /      |
             1 ----- 0
        */

        Block* top_block = chunk->get_neighbour_block_from_top(block_coords);
        submit_block_face(block_coords, block_info.top_texture_id, BlockFaceId_Top, 0, 1, 2, 3, chunk, block, top_block);
        /*
            bottom face

             7 ----- 6
            |      /  |
            |     /   |
            |    /    |
            |   /     |
            |  /      |
             4 ----- 5
        */

        Block* bottom_block = chunk->get_neighbour_block_from_bottom(block_coords);
        submit_block_face(block_coords, block_info.bottom_texture_id, BlockFaceId_Bottom, 5, 4, 7, 6, chunk, block, bottom_block);

        /*
            left face

             2 ----- 1
            |      /  |
            |     /   |
            |    /    |
            |   /     |
            |  /      |
             6 ----- 5
        */
        Block* left_block = nullptr;

        if (block_coords.x == 0)
        {
            left_block = &(chunk->left_edge_blocks[block_coords.y * MC_CHUNK_DEPTH + block_coords.z]);
        }
        else
        {
            left_block = chunk->get_neighbour_block_from_left(block_coords);
        }
        submit_block_face(block_coords, block_info.side_texture_id, BlockFaceId_Left, 5, 6, 2, 1, chunk, block, left_block);

        /*
            right face

             0 ----- 3
            |      /  |
            |     /   |
            |    /    |
            |   /     |
            |  /      |
             4 ----- 7
        */

        Block* right_block = nullptr;

        if (block_coords.x == MC_CHUNK_WIDTH - 1)
        {
            right_block = &(chunk->right_edge_blocks[block_coords.y * MC_CHUNK_DEPTH + block_coords.z]);
        }
        else
        {
            right_block = chunk->get_neighbour_block_from_right(block_coords);
        }
        submit_block_face(block_coords, block_info.side_texture_id, BlockFaceId_Right, 7, 4, 0, 3, chunk, block, right_block);

        /*
            front face

             3 ----- 2
            |      /  |
            |     /   |
            |    /    |
            |   /     |
            |  /      |
             7 ----- 6
        */
        Block* front_block = nullptr;

        if (block_coords.z == 0)
        {
            front_block = &(chunk->top_edge_blocks[block_coords.y * MC_CHUNK_WIDTH + block_coords.x]);
        }
        else
        {
            front_block = chunk->get_neighbour_block_from_front(block_coords);
        }
        submit_block_face(block_coords, block_info.side_texture_id, BlockFaceId_Front, 6, 7, 3, 2, chunk, block, front_block);

        /*
            back face

            (22) 1 ----- 0 (23)
                |      /  |
                |     /   |
                |    /    |
                |   /     |
                |  /      |
            (21) 5 ----- 4 (20)
        */

        Block* back_block = nullptr;

        if (block_coords.z == MC_CHUNK_DEPTH - 1)
        {
            back_block = &(chunk->bottom_edge_blocks[block_coords.y * MC_CHUNK_WIDTH + block_coords.x]);
        }
        else
        {
            back_block = chunk->get_neighbour_block_from_back(block_coords);
        }

        submit_block_face(block_coords, block_info.side_texture_id, BlockFaceId_Back, 4, 5, 1, 0, chunk, block, back_block);
    }

    void Opengl_Renderer::free_chunk(Chunk* chunk)
    {
        Chunk_Render_Data& render_data = chunk->render_data;

        if (render_data.vertex_array_id)
        {
            glDeleteBuffers(1, &render_data.vertex_buffer_id);
            glDeleteVertexArrays(1, &render_data.vertex_array_id);
            render_data.vertex_array_id = 0;
            render_data.vertex_buffer_id = 0;
        }
    }

    void Opengl_Renderer::prepare_chunk_for_rendering(Chunk *chunk)
    {
        for (i32 y = 0; y < MC_CHUNK_HEIGHT; ++y)
        {
            for (i32 z = 0; z < MC_CHUNK_DEPTH; ++z)
            {
                for (i32 x = 0; x < MC_CHUNK_WIDTH; ++x)
                {
                    glm::ivec3 block_coords = { x, y, z };
                    Block* block = chunk->get_block(block_coords);

                    if (block->id == BlockId_Air)
                    {
                        continue;
                    }

                    submit_block(chunk, block, block_coords);
                }
            }
        }

        chunk->loaded = true;
    }

    void Opengl_Renderer::upload_chunk_to_gpu(Chunk *chunk)
    {
        assert(chunk->loaded && !chunk->ready_for_rendering);

        Chunk_Render_Data& render_data = chunk->render_data;

        glGenVertexArrays(1, &render_data.vertex_array_id);
        glBindVertexArray(render_data.vertex_array_id);

        glGenBuffers(1, &render_data.vertex_buffer_id);
        glBindBuffer(GL_ARRAY_BUFFER, render_data.vertex_buffer_id);
        u32 vertex_count = render_data.vertex_count;
        glBufferData(GL_ARRAY_BUFFER,
                     vertex_count * sizeof(Vertex),
                     render_data.vertices,
                     GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribIPointer(0,
                               1,
                               GL_UNSIGNED_INT,
                               sizeof(Vertex),
                               (const void*)offsetof(Vertex, data0));

        glEnableVertexAttribArray(1);
        glVertexAttribIPointer(1,
                              1,
                              GL_UNSIGNED_INT,
                              sizeof(Vertex),
                              (const void*)offsetof(Vertex, data1));

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        chunk->ready_for_rendering = true;
    }

    void Opengl_Renderer::render_chunk(Chunk *chunk, Opengl_Shader *shader)
    {
        shader->use();
        shader->set_uniform_vec3("u_chunk_position",
                                 chunk->position.x,
                                 chunk->position.y,
                                 chunk->position.z);

        Chunk_Render_Data& render_data = chunk->render_data;

        glBindVertexArray(render_data.vertex_array_id);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, internal_data.chunk_index_buffer_id);
        glDrawElements(GL_TRIANGLES, render_data.face_count * 6, GL_UNSIGNED_INT, (const void*)0);
    }

    void Opengl_Renderer::begin(
        const glm::vec4& clear_color,
        Camera *camera,
        Opengl_Shader *shader)
    {
        glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader->use();
        shader->set_uniform_mat4("u_view", glm::value_ptr(camera->view));
        shader->set_uniform_mat4("u_projection", glm::value_ptr(camera->projection));

        f32 rgba_color_factor = 1.0f / 255.0f;
        glm::vec4 grass_color  = { 109.0f, 184.0f, 79.0f, 255.0f };
        grass_color *= rgba_color_factor;

        shader->set_uniform_vec4(
            "u_biome_color",
            grass_color.r,
            grass_color.g,
            grass_color.b,
            grass_color.a);

        shader->set_uniform_i32("u_block_sprite_sheet", 0);
        internal_data.block_sprite_sheet.bind(0);

        shader->set_uniform_i32("u_uvs", 1);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_BUFFER, internal_data.uv_texture_id);
    }

    void Opengl_Renderer::end()
    {
        internal_data.platform->opengl_swap_buffers();

#ifndef MC_DIST
        if (internal_data.should_print_stats)
        {
            fprintf(
                stderr,
                "Opengl Renderer Stats\nBlock Count: %u\nFace Count: %u\nDraw Calls: %u\n---------------\n",
                internal_data.stats.block_count,
                internal_data.stats.block_face_count,
                internal_data.stats.draw_count);
            memset(&internal_data.stats, 0, sizeof(Opengl_Renderer_Stats));
        }
#endif
    }

    void APIENTRY gl_debug_output(GLenum source,
                                  GLenum type,
                                  u32 id,
                                  GLenum severity,
                                  GLsizei length,
                                  const char *message,
                                  const void *user_param)
    {
        if (id == 131169 || id == 131185 || id == 131218 || id == 131204)
        {
            return;
        }

        const char *source_cstring = nullptr;

        switch (source)
        {
            case GL_DEBUG_SOURCE_API: source_cstring = "API"; break;
            case GL_DEBUG_SOURCE_WINDOW_SYSTEM: source_cstring = "Window System"; break;
            case GL_DEBUG_SOURCE_SHADER_COMPILER: source_cstring = "Shader Compiler"; break;
            case GL_DEBUG_SOURCE_THIRD_PARTY: source_cstring = "Third Party"; break;
            case GL_DEBUG_SOURCE_APPLICATION: source_cstring = "Application"; break;
            case GL_DEBUG_SOURCE_OTHER: source_cstring = "Other"; break;
        }

        const char *type_cstring = nullptr;

        switch (type)
        {
            case GL_DEBUG_TYPE_ERROR: type_cstring = "Error"; break;
            case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: type_cstring = "Deprecated Behaviour"; break;
            case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: type_cstring = "Undefined Behaviour"; break;
            case GL_DEBUG_TYPE_PORTABILITY: type_cstring = "Portability"; break;
            case GL_DEBUG_TYPE_PERFORMANCE: type_cstring = "Performance"; break;
            case GL_DEBUG_TYPE_MARKER: type_cstring = "Marker"; break;
            case GL_DEBUG_TYPE_PUSH_GROUP: type_cstring = "Push Group"; break;
            case GL_DEBUG_TYPE_POP_GROUP: type_cstring = "Pop Group"; break;
            case GL_DEBUG_TYPE_OTHER: type_cstring = "Other"; break;
        }

        const char *severity_cstring = nullptr;

        switch (severity)
        {
            case GL_DEBUG_SEVERITY_HIGH:         severity_cstring = "High"; break;
            case GL_DEBUG_SEVERITY_MEDIUM:       severity_cstring = "Medium"; break;
            case GL_DEBUG_SEVERITY_LOW:          severity_cstring = "Low"; break;
            case GL_DEBUG_SEVERITY_NOTIFICATION: severity_cstring = "Notification"; break;
        }

        fprintf(stderr, "OpenGL Debug Message\n");
        fprintf(stderr, "[Source]: %s\n", source_cstring);
        fprintf(stderr, "[Severity]: %s\n", severity_cstring);
        fprintf(stderr, "[%s]: %s\n", type_cstring, message);

        auto data = (Opengl_Renderer_Data *)user_param;

#ifndef MC_DIST
        if (data->should_trace_debug_messsage)
        {
            MC_DebugBreak();
        }
#endif
    }

    Opengl_Renderer_Data Opengl_Renderer::internal_data;
}