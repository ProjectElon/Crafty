#include "game/visual_debugging.h"

#include "memory/memory_arena.h"
#include "game/world.h"
#include "game/game.h"
#include "core/input.h"
#include "ui/ui.h"
#include "renderer/opengl_renderer.h"
#include "renderer/opengl_debug_renderer.h"
#include "renderer/opengl_2d_renderer.h"

namespace minecraft {

    void collect_visual_debugging_data(Game_Debug_State      *debug_state,
                                       Game_State            *game_state,
                                       Select_Block_Result   *select_query,
                                       Temprary_Memory_Arena *frame_arena)
    {
        World *world             = game_state->world;
        Game_Config *game_config = &game_state->game_config;

        Camera *camera = &game_state->camera;

        if (is_block_query_valid(select_query->block_query) &&
            is_block_query_valid(select_query->block_facing_normal_query))
        {
            glm::vec3 abs_normal  = glm::abs(select_query->normal);
            glm::vec4 debug_color = { abs_normal.x, abs_normal.y, abs_normal.z, 1.0f };

            opengl_debug_renderer_push_cube(select_query->block_facing_normal_position,
                                            { 0.5f, 0.5f, 0.5f },
                                            debug_color);

            opengl_debug_renderer_push_line(select_query->block_position,
                                            select_query->block_position + select_query->normal * 1.5f,
                                            debug_color);

            glm::ivec2 chunk_coords = select_query->block_facing_normal_query.chunk->world_coords;
            glm::ivec3 block_coords = select_query->block_facing_normal_query.block_coords;

            const char *back_facing_normal_label = "";

            switch (select_query->face)
            {
                case BlockFace_Top:
                {
                    back_facing_normal_label = "top";
                } break;

                case BlockFace_Bottom:
                {
                    back_facing_normal_label = "bottom";
                } break;

                case BlockFace_Front:
                {
                    back_facing_normal_label = "front";
                } break;

                case BlockFace_Back:
                {
                    back_facing_normal_label = "back";
                } break;

                case BlockFace_Left:
                {
                    back_facing_normal_label = "left";
                } break;

                case BlockFace_Right:
                {
                    back_facing_normal_label = "right";
                } break;
            }

            debug_state->block_facing_normal_face_text =
                push_formatted_string8_null_terminated(frame_arena,
                                                       "block face: %s",
                                                       back_facing_normal_label);

            debug_state->block_facing_normal_chunk_coords_text =
                push_formatted_string8_null_terminated(frame_arena,
                                                       "chunk: (%d, %d)",
                                                       chunk_coords.x,
                                                       chunk_coords.y);

            debug_state->block_facing_normal_block_coords_text =
                push_formatted_string8_null_terminated(frame_arena,
                                                       "block: (%d, %d, %d)",
                                                        block_coords.x,
                                                        block_coords.y,
                                                        block_coords.z);

            Block_Light_Info* light_info = get_block_light_info(select_query->block_facing_normal_query.chunk,
                                                                select_query->block_facing_normal_query.block_coords);

            i32 sky_light_level = get_sky_light_level(world, light_info);

            debug_state->block_facing_normal_sky_light_level_text =
                push_formatted_string8_null_terminated(frame_arena,
                                                       "sky light level: %d",
                                                       sky_light_level);

            debug_state->block_facing_normal_light_source_level_text =
                push_formatted_string8_null_terminated(frame_arena,
                                                       "light source level: %d",
                                                       (i32)light_info->light_source_level);

            debug_state->block_facing_normal_light_level_text =
                push_formatted_string8_null_terminated(frame_arena,
                                                       "light level: %d",
                                                       glm::max(sky_light_level, (i32)light_info->light_source_level));

        }

        const Opengl_Renderer_Stats *stats = opengl_renderer_get_stats();

        debug_state->frames_per_second_text =
            push_formatted_string8_null_terminated(frame_arena,
                                                   "FPS: %d",
                                                   game_state->frames_per_second);

        debug_state->frame_time_text =
            push_formatted_string8_null_terminated(frame_arena,
                                                   "frame time: %.2f ms",
                                                   game_state->delta_time * 1000.0f);

        debug_state->vertex_count_text =
            push_formatted_string8_null_terminated(frame_arena,
                                                   "vertex count: %d",
                                                   stats->per_frame.face_count * 4);

        debug_state->face_count_text =
            push_formatted_string8_null_terminated(frame_arena,
                                                   "face count: %u",
                                                   stats->per_frame.face_count);

        debug_state->sub_chunk_bucket_capacity_text =
            push_formatted_string8_null_terminated(frame_arena,
                                                   "sub chunk bucket capacity: %llu",
                                                   World::sub_chunk_bucket_capacity);

        i64 sub_chunk_bucket_count = World::sub_chunk_bucket_capacity - opengl_renderer_get_free_chunk_bucket_count();
        debug_state->sub_chunk_bucket_count_text =
            push_formatted_string8_null_terminated(frame_arena,
                                                   "sub chunk buckets: %llu",
                                                   sub_chunk_bucket_count);

        {
            f64 total_size =
                (World::sub_chunk_bucket_capacity * World::sub_chunk_bucket_size) / (1024.0 * 1024.0);

            debug_state->sub_chunk_bucket_total_memory_text =
                push_formatted_string8_null_terminated(frame_arena,
                                                       "buckets total memory: %.2f",
                                                       total_size);
        }

        {
            f64 total_size =
                (sub_chunk_bucket_count * World::sub_chunk_bucket_size) / (1024.0 * 1024.0);
            debug_state->sub_chunk_bucket_allocated_memory_text =
                push_formatted_string8_null_terminated(frame_arena,
                                                       "buckets allocated memory: %.2f",
                                                       total_size);
        }

        {
            f64 total_size =
                stats->persistent.sub_chunk_used_memory / (1024.0 * 1024.0);
            debug_state->sub_chunk_bucket_used_memory_text =
                push_formatted_string8_null_terminated(frame_arena,
                                                       "buckets used memory: %.2f",
                                                       total_size);
        }

        debug_state->player_position_text =
            push_formatted_string8_null_terminated(frame_arena,
                                                   "position: (%.2f, %.2f, %.2f)",
                                                   camera->position.x,
                                                   camera->position.y,
                                                   camera->position.z);

        glm::vec2 active_chunk_coords = world_position_to_chunk_coords(camera->position);

        debug_state->player_chunk_coords_text =
            push_formatted_string8_null_terminated(frame_arena,
                                                   "chunk coords: (%d, %d)",
                                                   (i32)active_chunk_coords.x,
                                                   (i32)active_chunk_coords.y);

        debug_state->chunk_radius_text =
            push_formatted_string8_null_terminated(frame_arena,
                                                   "chunk radius: %d",
                                                   game_config->chunk_radius);

        debug_state->global_sky_light_level_text =
            push_formatted_string8_null_terminated(frame_arena,
                                                   "global sky light level: %d",
                                                   (u32)world->sky_light_level);

        u32 hours;
        u32 minutes;
        u32 seconds;
        game_time_to_real_time(world->game_time, &hours, &minutes, &seconds);
        debug_state->game_time_text =
            push_formatted_string8_null_terminated(frame_arena,
                                                   "game time: %d:%d:%d",
                                                   hours,
                                                   minutes,
                                                   seconds);
    }

    void draw_visual_debugging_data(Game_Debug_State *debug_state,
                                    Input            *input,
                                    glm::vec2         frame_buffer_size)
    {
        UI::begin(input);

        UI::set_offset({ 10.0f, 10.0f });
        UI::set_fill_color({ 0.0f, 0.0f, 0.0f, 0.8f });
        UI::set_text_color({ 1.0f, 1.0f, 1.0f, 1.0f });

        UI::rect(frame_buffer_size * glm::vec2(0.33f, 1.0f) - glm::vec2(0.0f, 20.0f));
        UI::set_cursor({ 10.0f, 10.0f });

        UI::text(debug_state->player_chunk_coords_text);
        UI::text(debug_state->player_position_text);
        UI::text(debug_state->chunk_radius_text);

        String8 empty = Str8("");
        UI::text(empty);

        UI::text(debug_state->frames_per_second_text);
        UI::text(debug_state->frame_time_text);
        UI::text(debug_state->face_count_text);
        UI::text(debug_state->vertex_count_text);
        UI::text(debug_state->sub_chunk_bucket_capacity_text);
        UI::text(debug_state->sub_chunk_bucket_count_text);
        UI::text(debug_state->sub_chunk_bucket_total_memory_text);
        UI::text(debug_state->sub_chunk_bucket_allocated_memory_text);
        UI::text(debug_state->sub_chunk_bucket_used_memory_text);

        UI::text(empty);

        UI::text(debug_state->game_time_text);
        UI::text(debug_state->global_sky_light_level_text);

        UI::text(empty);

        String8 debug_block_label = Str8("debug block");
        UI::text(debug_block_label);
        UI::text(debug_state->block_facing_normal_chunk_coords_text);
        UI::text(debug_state->block_facing_normal_block_coords_text);
        UI::text(debug_state->block_facing_normal_face_text);
        UI::text(debug_state->block_facing_normal_sky_light_level_text);
        UI::text(debug_state->block_facing_normal_light_source_level_text);
        UI::text(debug_state->block_facing_normal_light_level_text);

        UI::text(empty);

        bool fxaa_enabled = opengl_renderer_is_fxaa_enabled();
        String8 fxaa_label = {};
        if (fxaa_enabled)
        {
            fxaa_label = Str8("FXAA: ON");
        }
        else
        {
            fxaa_label = Str8("FXAA: OFF");
        }
        UI::text(fxaa_label);
        UI::end();
    }
}