#pragma once

#include "core/common.h"
#include "core/event.h"
#include "core/platform.h"

#include <glm/glm.hpp>
#include <vector>
#include <atomic>

#define OPENGL_DEBUGGING 0
#define OPENGL_TRACE_DEBUG_MESSAGE 0

namespace minecraft {

    struct World;
    struct Platform;
    struct Camera;
    struct Memory_Arena;
    struct Opengl_Texture;
    struct Opengl_Shader;

    struct PerFrame_Stats
    {
        i32 face_count;
        i32 sub_chunk_count;
    };

    struct Persistent_Stats
    {
        std::atomic< u64 > sub_chunk_used_memory;
    };

    struct Opengl_Renderer_Stats
    {
        PerFrame_Stats   per_frame;
        Persistent_Stats persistent;
    };

    bool initialize_opengl_renderer(GLFWwindow   *window,
                                    u32           initial_frame_buffer_width,
                                    u32           initial_frame_buffer_height,
                                    Memory_Arena *Arena);

    void shutdown_opengl_renderer();

    bool opengl_renderer_on_resize(const Event* event, void *sender);

    void opengl_renderer_update_sub_chunk(World *world,
                                          Chunk* chunk,
                                          u32 sub_chunk_index);

    void opengl_renderer_begin_frame(const glm::vec4& clear_color,
                                     const glm::vec4& tint_color,
                                     Camera *camera);

    void opengl_renderer_render_sub_chunk(World *world,
                                          Chunk *chunk,
                                          u32 sub_chunk_index);

    void opengl_renderer_render_chunks_at_region(World *world,
                                                 const World_Region_Bounds &player_region_bounds,
                                                 Camera *camera);

    void opengl_renderer_end_frame(i32 chunk_radius,
                                   f32 sky_light_level,
                                   Block_Query_Result *selected_block_query);

    void opengl_renderer_swap_buffers(struct GLFWwindow *window);

    void opengl_renderer_allocate_sub_chunk_bucket(Sub_Chunk_Bucket *bucket);
    void opengl_renderer_reset_sub_chunk_bucket(Sub_Chunk_Bucket *bucket);
    void opengl_renderer_free_sub_chunk_bucket(Sub_Chunk_Bucket *bucket);

    i32  opengl_renderer_allocate_sub_chunk_instance();
    void opengl_renderer_free_sub_chunk_instance(i32 instance_memory_id);

    void opengl_renderer_free_sub_chunk(Chunk *chunk, u32 sub_chunk_index);

    void opengl_renderer_upload_sub_chunk_to_gpu(World *world,
                                                 Chunk *chunk,
                                                 u32 sub_chunk_index);

    bool opengl_renderer_recreate_frame_buffers();

    glm::vec2 opengl_renderer_get_frame_buffer_size();
    const Opengl_Renderer_Stats *opengl_renderer_get_stats();
    i64 opengl_renderer_get_free_chunk_bucket_count();

    bool opengl_renderer_is_fxaa_enable();
    void opengl_renderer_toggle_fxaa();

    // todo(harlequin): temprary use game assets
    Opengl_Texture *opengl_renderer_get_block_sprite_sheet_texture();
}