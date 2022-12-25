#pragma once

#include "core/common.h"
#include "core/event.h"
#include "core/input.h"

#include "memory/memory_arena.h"

#include "containers/string.h"

#include "renderer/camera.h"

#include "game/world.h"
#include "game/inventory.h"

struct GLFWwindow;

namespace minecraft {

    enum WindowMode
    {
        WindowMode_None,
        WindowMode_Fullscreen,
        WindowMode_BorderlessFullscreen,
        WindowMode_Windowed
    };

    struct Game_Config
    {
        const char *window_title;
        i32         window_x;
        i32         window_y;
        i32         window_x_before_fullscreen;
        i32         window_y_before_fullscreen;
        u32         window_width;
        u32         window_height;
        WindowMode  window_mode;
        bool        is_cursor_visible;
        bool        is_raw_mouse_motion_enabled;
    };

    struct Game_Memory
    {
        u64          permanent_memory_size;
        void        *permanent_memory;

        u64          transient_memory_size;
        void        *transient_memory;

        Memory_Arena permanent_arena;
        Memory_Arena transient_arena;
    };

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

    struct Game_State
    {
        Game_Memory *game_memory;
        Game_Config  game_config;
        GLFWwindow  *window;

        Event_System event_system;
        Input        game_input;
        Inventory    inventory;
        Camera       camera;

        World        *world;

        bool         is_minimized;
        bool         is_running;
        bool         is_inventory_active;
        bool         is_cursor_locked;

        // todo(harlequin): game_assets
        void        *ingame_crosshair_texture;
        void        *inventory_crosshair_texture;

        bool             is_visual_debugging_enabled;
        Game_Debug_State debug_state;
    };

    bool initialize_game(Game_State *game_state);
    void shutdown_game(Game_State *game_state);
    void run_game(Game_State *game_state);

    void toggle_visual_debugging(Game_State *game_state);
    void toggle_inventory(Game_State *game_state);
}