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

    enum BlockFaceCornerId : u32
    {
        BlockFaceCornerId_BottomRight  = 0,
        BlockFaceCornerId_BottomLeft   = 1,
        BlockFaceCornerId_TopLeft      = 2,
        BlockFaceCornerId_TopRight     = 3
    };

    struct Block_Face_Info
    {
        glm::vec3 normal;
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
        u32 chunk_index_buffer_id;

        Opengl_Texture block_sprite_sheet;
        u32 uv_buffer_id;
        u32 uv_texture_id;

#ifndef MC_DIST
        Opengl_Renderer_Stats stats;
        bool should_trace_debug_messsage = true;
        bool should_print_stats = false;
#endif
    };

    struct Opengl_Renderer
    {
        static Opengl_Renderer_Data internal_data;

        Opengl_Renderer() = delete;

        static bool initialize(Platform *platform);
        static void shutdown();

        static bool on_resize(const Event* event, void *sender);

        static u32 compress_vertex(const glm::ivec3& block_coords,
            u32 local_position_id,
            u32 face_id,
            u32 face_corner_id,
            u32 flags);

        static void extract_vertex(u32 vertex,
            glm::ivec3& block_coords,
            u32& out_local_position_id,
            u32& out_face_id,
            u32& out_face_corner_id,
            u32& out_flags);

        static void free_chunk(Chunk* chunk);
        static void upload_chunk_to_gpu(Chunk *chunk);

        static void begin(
            const glm::vec4& clear_color,
            Camera *camera,
            Opengl_Shader *shader);

        static void render_chunk(Chunk *chunk, Opengl_Shader *shader);
        
        static void end();
    };
}