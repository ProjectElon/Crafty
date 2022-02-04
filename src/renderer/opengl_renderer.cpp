#include "opengl_renderer.h"
#include "core/platform.h"
#include "camera.h"
#include "opengl_shader.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include "meta/spritesheet_meta.h"

namespace minecraft {

    #define MC_MAX_BLOCK_COUNT_PER_PATCH 16384
    #define MC_VERTEX_SIZE_PER_PATCH MC_MAX_BLOCK_COUNT_PER_PATCH * sizeof(Block_Vertex) * 24
    #define MC_INDEX_SIZE_PER_PATCH MC_MAX_BLOCK_COUNT_PER_PATCH * sizeof(u32) * 36

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

#if 1
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
        glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // multisampling
        glEnable(GL_MULTISAMPLE);

        // face culling
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        internal_data.platform = platform;
        internal_data.max_block_count_per_patch = MC_MAX_BLOCK_COUNT_PER_PATCH;
        internal_data.current_block_face_count = 0;

        // todo(harlequin): memory system
        internal_data.base_block_vertex_pointer = new Block_Vertex[MC_MAX_BLOCK_COUNT_PER_PATCH * 24]; // 6 faces each has 4 verties
        internal_data.current_block_vertex_pointer = internal_data.base_block_vertex_pointer;

        // todo(harlequin): memory system
        u32 *indicies = new u32[MC_MAX_BLOCK_COUNT_PER_PATCH * 36]; // 6 faces each face has 2 triangles
        
        defer
        {
            delete[] indicies;
        };

        u32 element_index = 0;
        u32 vertex_index = 0;
        
        for (u32 i = 0; i < MC_MAX_BLOCK_COUNT_PER_PATCH * 6; i++)
        {
            indicies[element_index + 0] = vertex_index + 3;
            indicies[element_index + 1] = vertex_index + 1;
            indicies[element_index + 2] = vertex_index + 0;
            
            indicies[element_index + 3] = vertex_index + 3;
            indicies[element_index + 4] = vertex_index + 2;
            indicies[element_index + 5] = vertex_index + 1;

            element_index += 6;
            vertex_index += 4;
        }

        glGenVertexArrays(1, &internal_data.block_vao);
        glBindVertexArray(internal_data.block_vao);

        glGenBuffers(1, &internal_data.block_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, internal_data.block_vbo);
        glBufferData(GL_ARRAY_BUFFER, MC_VERTEX_SIZE_PER_PATCH, nullptr, GL_DYNAMIC_DRAW);

        glGenBuffers(1, &internal_data.block_ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, internal_data.block_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, MC_INDEX_SIZE_PER_PATCH, indicies, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0, 
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Block_Vertex),
            (const void*)offsetof(Block_Vertex, position));

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1,
            2,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Block_Vertex),
            (const void*)offsetof(Block_Vertex, uv));

        glEnableVertexAttribArray(2);
        glVertexAttribIPointer(
            2,
            1,
            GL_UNSIGNED_INT,
            sizeof(Block_Vertex),
            (const void*)offsetof(Block_Vertex, face));

        glEnableVertexAttribArray(3);
        glVertexAttribIPointer(
            3,
            1,
            GL_UNSIGNED_INT,
            sizeof(Block_Vertex),
            (const void*)offsetof(Block_Vertex, should_color_top_by_biome));

        const char *block_sprite_sheet_path = "../assets/textures/spritesheet.png"; // todo(Harlequin): rename spritesheet to block_spritesheet
        bool success = internal_data.block_sprite_sheet.load_from_file(block_sprite_sheet_path);

        if (!success)
        {
            fprintf(stderr, "[ERROR]: failed to load texture at %s\n", block_sprite_sheet_path);
            return false;
        }

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
        
        // color from link: https://www.pngkit.com/view/u2r5y3e6w7y3a9w7_biome-minecraft-biome-color-chart/
        f32 rgba_color_factor = 1.0f / 255.0f;
        glm::vec4 forest_color = { 0.0f, 152.0f, 33, 255 };
        glm::vec4 grass_color = { 130, 203, 67, 255 };
        forest_color *= rgba_color_factor;
        grass_color *= rgba_color_factor;

        shader->set_uniform_vec4(
            "u_biome_color",
            grass_color.r,
            grass_color.g,
            grass_color.b,
            grass_color.a);
        
        shader->set_uniform_i32("u_block_sprite_sheet", 0);
        internal_data.block_sprite_sheet.bind(0);

        glBindVertexArray(internal_data.block_vao);
    }

    void Opengl_Renderer::submit_block_face(
        const glm::vec3& p0,
        const glm::vec3& p1,
        const glm::vec3& p2,
        const glm::vec3& p3,
        const UV_Rect& uv_rect, 
        BlockFaceId face,
        Block* block,
        Block* block_facing_normal)
    {
        const Block_Info& block_info = World::block_infos[block->id];
        bool should_color_top_by_biome = block_info.flags & BlockFlags_Should_Color_Top_By_Biome;

        const Block_Info& block_facing_normal_info = World::block_infos[block_facing_normal->id];
        bool block_facing_normal_is_solid = block_facing_normal_info.flags & BlockFlags_Is_Solid;
        bool block_facing_normal_is_transparent = block_facing_normal_info.flags & BlockFlags_Is_Transparent;

        if (!block_facing_normal_is_solid || block_facing_normal_is_transparent)
        {
            *internal_data.current_block_vertex_pointer = { p0, uv_rect.bottom_right, face, should_color_top_by_biome };
            internal_data.current_block_vertex_pointer++;

            *internal_data.current_block_vertex_pointer = { p1, uv_rect.bottom_left, face, should_color_top_by_biome };
            internal_data.current_block_vertex_pointer++;

            *internal_data.current_block_vertex_pointer = { p2, uv_rect.top_left, face, should_color_top_by_biome };
            internal_data.current_block_vertex_pointer++;

            *internal_data.current_block_vertex_pointer = { p3, uv_rect.top_right, face, should_color_top_by_biome };
            internal_data.current_block_vertex_pointer++;

            internal_data.current_block_face_count++;
            
            if (internal_data.current_block_face_count == internal_data.max_block_count_per_patch * 6)
            {
                flush_patch();
            }
        }
    }

    void Opengl_Renderer::submit_block(
        Chunk *chunk,
        Block *block,
        const glm::ivec3 &block_coords)
    {
        const Block_Info& block_info = World::block_infos[block->id];

        UV_Rect& top_face_uv    = texture_uv_rects[block_info.top_texture_id];
        UV_Rect& bottom_face_uv = texture_uv_rects[block_info.bottom_texture_id];
        UV_Rect& side_face_uv   = texture_uv_rects[block_info.side_texture_id];

         /*
          1----------2
          |\         |\
          | 0--------|-3
          | |        | |
          5-|--------6 |
           \|         \|
            4----------7
        */
        glm::vec3 position = chunk->get_block_position(block_coords);
                    
        glm::vec3 p0 = position + glm::vec3(0.5f,   0.5f,  0.5f);
        glm::vec3 p1 = position + glm::vec3(-0.5f,  0.5f,  0.5f);
        glm::vec3 p2 = position + glm::vec3(-0.5f,  0.5f, -0.5f);
        glm::vec3 p3 = position + glm::vec3(0.5f,   0.5f, -0.5f);
        glm::vec3 p4 = position + glm::vec3(0.5f,  -0.5f,  0.5f);
        glm::vec3 p5 = position + glm::vec3(-0.5f, -0.5f,  0.5f);
        glm::vec3 p6 = position + glm::vec3(-0.5f, -0.5f, -0.5f);
        glm::vec3 p7 = position + glm::vec3(0.5f,  -0.5f, -0.5f);
        
        /*
            top face

            (2)  2 ----- 3 (3)
                |      /  |
                |     /   |
                |    /    |
                |   /     |
                |  /      |
            (1)  1 ----- 0 (0)
        */

        Block* top_block = chunk->get_neighbour_block_from_top(block_coords);
        submit_block_face(p0, p1, p2, p3, top_face_uv, BlockFaceId_Top, block, top_block);

        /*
            bottom face

            (6)  7 ----- 6 (7)
                |      /  |
                |     /   |
                |    /    |
                |   /     |
                |  /      |
            (5)  4 ----- 5 (4)
        */
        
        Block* bottom_block = chunk->get_neighbour_block_from_bottom(block_coords);
        submit_block_face(p5, p4, p7, p6, bottom_face_uv, BlockFaceId_Bottom, block, bottom_block);

        /*
            left face

            (10) 2 ----- 1 (11)
                |      /  |
                |     /   |
                |    /    |
                |   /     |
                |  /      |
            (9)  6 ----- 5 (8)
        */

        Block* left_block = chunk->get_neighbour_block_from_left(block_coords);
        submit_block_face(p5, p6, p2, p1, side_face_uv, BlockFaceId_Left, block, left_block);

        /*
            right face

            (14) 0 ----- 3 (15)
                |      /  |
                |     /   |
                |    /    |
                |   /     |
                |  /      |
            (13) 4 ----- 7 (12)
        */
        
        Block* right_block = chunk->get_neighbour_block_from_right(block_coords);
        submit_block_face(p7, p4, p0, p3, side_face_uv, BlockFaceId_Right, block, right_block);

        /*
            front face

            (18) 3 ----- 2 (19)
                |      /  |
                |     /   |
                |    /    |
                |   /     |
                |  /      |
            (17) 7 ----- 6 (16)
        */

        Block* front_block = chunk->get_neighbour_block_from_front(block_coords);
        submit_block_face(p6, p7, p3, p2, side_face_uv, BlockFaceId_Front, block, front_block);

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
        
        Block* back_block = chunk->get_neighbour_block_from_back(block_coords);
        submit_block_face(p4, p5, p1, p0, side_face_uv, BlockFaceId_Back, block, back_block);

        internal_data.stats.block_count++;
    }

    void Opengl_Renderer::flush_patch()
    {
        if (internal_data.current_block_face_count > 0)
        {
            glBufferSubData(GL_ARRAY_BUFFER,
                            0,
                            internal_data.current_block_face_count * sizeof(Block_Vertex) * 4,
                            internal_data.base_block_vertex_pointer);

            glDrawElements(GL_TRIANGLES,
                           internal_data.current_block_face_count * 6,
                           GL_UNSIGNED_INT,
                           (const void*)0);

            internal_data.stats.draw_count++;
            internal_data.stats.block_face_count += internal_data.current_block_face_count;

            internal_data.current_block_vertex_pointer = internal_data.base_block_vertex_pointer;
            internal_data.current_block_face_count = 0;
        }
    }

    void Opengl_Renderer::submit_chunk(Chunk *chunk)
    {
        for (u32 y = 0; y < MC_CHUNK_HEIGHT; ++y)
        {
            for (u32 z = 0; z < MC_CHUNK_DEPTH; ++z)
            {
                for (u32 x = 0; x < MC_CHUNK_WIDTH; ++x)
                {
                    Block &block = chunk->blocks[y][z][x];
                    
                    if (block.id == BlockId_Air)
                    { 
                        continue;
                    }
                    
                    glm::ivec3 block_coords = { x, y, z };
                    submit_block(chunk, &block, block_coords);
                }
            }
        }
    }

    void Opengl_Renderer::end()
    {
        flush_patch();
        internal_data.platform->opengl_swap_buffers();
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

        if (data->should_trace_debug_messsage)
        {
            MC_DebugBreak();
        }
    }

    Opengl_Renderer_Data Opengl_Renderer::internal_data;
}