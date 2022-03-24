#pragma once

#include "core/common.h"
#include "core/event.h"

#include "opengl_texture.h"
#include "game/world.h"

#include <glm/glm.hpp>
#include <vector>
#include <mutex>

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

    struct Draw_Elements_Indirect_Command
    {
        u32 count;
        u32 instanceCount;
        u32 firstIndex;
        u32 baseVertex;
        u32 baseInstance;
    };

    struct Opengl_Renderer_Data
    {
        Platform *platform;

        u32 chunk_vertex_array_id;
        u32 chunk_vertex_buffer_id;
        u32 chunk_instance_buffer_id;
        u32 chunk_index_buffer_id;

        Sub_Chunk_Vertex *base_vertex;
        Sub_Chunk_Instance *base_instance;

        std::mutex free_sub_chunks_mutex;
        std::vector<u32> free_sub_chunks;

        Opengl_Texture block_sprite_sheet;
        u32 uv_buffer_id;
        u32 uv_texture_id;

        u32 command_count;
        Draw_Elements_Indirect_Command command_buffer[World::sub_chunk_capacity];
        u32 command_buffer_id;

        bool should_trace_debug_messsage = true;
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

        static void free_sub_chunk(Chunk* chunk, u32 sub_chunk_index);

        static void upload_sub_chunk_to_gpu(Chunk *chunk, u32 sub_chunk_index);
        static void update_sub_chunk(Chunk* chunk, u32 sub_chunk_index);

        static void begin(
            const glm::vec4& clear_color,
            Camera *camera,
            Opengl_Shader *shader);

        static void render_sub_chunk(Chunk *chunk, u32 sub_chunk_index, Opengl_Shader *shader);
        
        static void end();

        static void swap_buffers();
    };
}