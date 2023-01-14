#pragma once

#include "core/common.h"
#include "containers/string.h"

#include <glm/glm.hpp>

namespace minecraft {

    struct Input;
    struct Game_State;
    struct Select_Block_Result;
    struct Temprary_Memory_Arena;

    struct Game_Debug_State
    {
        String8 frames_per_second_text;
        String8 frame_time_text;
        String8 vertex_count_text;
        String8 face_count_text;
        String8 sub_chunk_bucket_capacity_text;
        String8 sub_chunk_bucket_count_text;
        String8 sub_chunk_bucket_total_memory_text;
        String8 sub_chunk_bucket_allocated_memory_text;
        String8 sub_chunk_bucket_used_memory_text;
        String8 player_position_text;
        String8 player_chunk_coords_text;
        String8 chunk_radius_text;
        String8 game_time_text;
        String8 global_sky_light_level_text;
        String8 block_facing_normal_chunk_coords_text;
        String8 block_facing_normal_block_coords_text;
        String8 block_facing_normal_face_text;
        String8 block_facing_normal_sky_light_level_text;
        String8 block_facing_normal_light_source_level_text;
        String8 block_facing_normal_light_level_text;
    };

    void collect_visual_debugging_data(Game_Debug_State      *debug_state,
                                       Game_State            *game_state,
                                       Select_Block_Result   *select_query,
                                       Temprary_Memory_Arena *frame_arena);

    void draw_visual_debugging_data(Game_Debug_State *debug_state,
                                    Input            *input,
                                    glm::vec2         frame_buffer_size);
}