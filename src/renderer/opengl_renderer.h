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

    struct Opengl_Renderer_Stats
    {
        i32 face_count;
        i32 sub_chunk_count;
    };

    struct Opengl_Renderer_Data
    {
        Platform *platform;

        glm::vec2 frame_buffer_size;
        u32 chunk_vertex_array_id;
        u32 chunk_vertex_buffer_id;
        u32 chunk_instance_buffer_id;
        u32 chunk_index_buffer_id;

        Sub_Chunk_Vertex *base_vertex;
        Sub_Chunk_Instance *base_instance;

        std::mutex free_buckets_mutex;
        std::vector<i32> free_buckets;

        std::mutex free_instances_mutex;
        std::vector<i32> free_instances;

        u32 opaque_command_buffer_id;
        u32 opaque_command_count;
        Draw_Elements_Indirect_Command opaque_command_buffer[World::sub_chunk_bucket_capacity];

        u32 transparent_command_buffer_id;
        u32 transparent_command_count;
        Draw_Elements_Indirect_Command transparent_command_buffer[World::sub_chunk_bucket_capacity];

        Opengl_Texture block_sprite_sheet;
        u32 uv_buffer_id;
        u32 uv_texture_id;

        Opengl_Renderer_Stats stats;
        i64 sub_chunk_used_memory;

        u16 sky_light_level;

        bool should_trace_debug_messsage = true;
    };

    struct Opengl_Renderer
    {
        static Opengl_Renderer_Data internal_data;

        Opengl_Renderer() = delete;

        static bool initialize(Platform *platform);
        static void shutdown();

        static bool on_resize(const Event* event, void *sender);

        static u32 compress_vertex0(const glm::ivec3& block_coords,
                                   u32 local_position_id,
                                   u32 face_id,
                                   u32 face_corner_id,
                                   u32 flags);

        static void extract_vertex0(u32 vertex,
                                   glm::ivec3& block_coords,
                                   u32& out_local_position_id,
                                   u32& out_face_id,
                                   u32& out_face_corner_id,
                                   u32& out_flags);

        static u32 compress_vertex1(u32 texture_uv_id, u32 light_level);
        static void extract_vertex1(u32 vertex, u32& out_texture_uv_id, u32 &out_light_level);

        static void allocate_sub_chunk_bucket(Sub_Chunk_Bucket *bucket);
        static void reset_sub_chunk_bucket(Sub_Chunk_Bucket *bucket);
        static void free_sub_chunk_bucket(Sub_Chunk_Bucket *bucket);

        static i32 allocate_sub_chunk_instance();
        static void free_sub_chunk_instance(i32 instance_memory_id);

        static void free_sub_chunk(Chunk* chunk, u32 sub_chunk_index);

        static void upload_sub_chunk_to_gpu(Chunk *chunk, u32 sub_chunk_index);
        static void update_sub_chunk(Chunk* chunk, u32 sub_chunk_index);

        static void begin(
            const glm::vec4& clear_color,
            Camera *camera,
            Opengl_Shader *shader);

        static void render_sub_chunk(Chunk *chunk, u32 sub_chunk_index, Opengl_Shader *shader);
        
        static void end();

        static void wait_for_gpu_to_finish_work();
        static void signal_gpu_for_work();

        static void swap_buffers();

        static inline glm::vec2 get_frame_buffer_size() { return internal_data.frame_buffer_size; }
    };
}