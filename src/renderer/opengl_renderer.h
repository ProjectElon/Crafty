#pragma once

#include "core/common.h"
#include "core/event.h"

#include <glm/glm.hpp>

#include "opengl_texture.h"
#include "game/world.h"

namespace minecraft {

    struct Platform;
    struct Camera;
    struct Opengl_Shader;

    enum BlockFaceId : u32
    {
        BlockFaceId_Top    = 0,
        BlockFaceId_Bottom = 1,
        BlockFaceId_Left   = 2,
        BlockFaceId_Right  = 3,
        BlockFaceId_Front  = 4,
        BlockFaceId_Back   = 5,
        BlockFaceId_Count  = 6
    };

    struct Block_Face_Info
    {
        glm::vec3 normal;
    };

    struct Block_Vertex
    {
        glm::vec3 position;
        glm::vec2 uv;

        // todo(harlequin): pack integer attributes
        u32 face;
        u32 should_color_top_by_biome;
    };

    struct Opengl_Renderer_Stats
    {
        u32 draw_count  = 0;
        u32 block_count = 0;
        u32 block_face_count = 0;
    };

    struct Opengl_Renderer_Data
    {
        Platform *platform;

        u32 max_block_count_per_patch;
        u32 current_block_face_count;
        
        Block_Vertex *base_block_vertex_pointer;
        Block_Vertex *current_block_vertex_pointer;

        u32 block_vao;
        u32 block_vbo;
        u32 block_ebo;

        Opengl_Texture block_sprite_sheet;

        Block_Face_Info block_face_infos[BlockFaceId_Count] = 
        {
            // Top
            {
                { 0.0f, 1.0f, 0.0f }
            },
            // Bottom
            {
                { 0.0f, -1.0f, 0.0f }
            },
            // Left
            {
                { -1.0f, 0.0f, 0.0f }
            },
            // Right
            {
                { 1.0f, 0.0f, 0.0f }
            },
            // Front
            {
                { 0.0f, 0.0f, -1.0f }
            },
            // Back
            {
                { 0.0f, 0.0f, 1.0f }
            }
        };

        // developer
        Opengl_Renderer_Stats stats;
        bool should_trace_debug_messsage = false;
        bool should_print_stats = false;
    };

    struct Opengl_Renderer
    {
        static Opengl_Renderer_Data internal_data;

        Opengl_Renderer() = delete;

        static bool initialize(Platform *platform);
        static void shutdown();

        static bool on_resize(const Event* event, void *sender);

        static void begin(
            const glm::vec4& clear_color,
            Camera *camera,
            Opengl_Shader *shader);
        
        static void submit_block_face(
            const glm::vec3& p0,
            const glm::vec3& p1,
            const glm::vec3& p2,
            const glm::vec3& p3,
            const UV_Rect& uv_rect,
            BlockFaceId face,
            Block* block,
            Block* block_facing_normal);

        static void submit_block( 
            Chunk *chunk,
            Block *block,
            const glm::ivec3& block_coords);

        static void submit_chunk(Chunk *chunk);
        static void flush_patch();
        
        static void end();
    };
}