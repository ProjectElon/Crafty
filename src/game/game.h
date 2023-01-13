#pragma once

#include "core/common.h"
#include "core/event.h"
#include "core/input.h"

#include "memory/memory_arena.h"

#include "containers/string.h"

#include "renderer/camera.h"

#include "game/game_config.h"
#include "game/ecs.h"
#include "game/world.h"
#include "game/inventory.h"
#include "game/game_assets.h"

#include "ui/dropdown_console.h"

struct GLFWwindow;

namespace minecraft {

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
        Game_Memory     *game_memory;
        const char      *config_file_path;
        Game_Config      game_config;
        GLFWwindow      *window;
        Event_System     event_system;
        Input            input;
        Input            gameplay_input;
        Input            inventory_input;
        Inventory        inventory;
        Camera           camera;
        Game_Assets      assets;
        Dropdown_Console console;
        World           *world;

        f32              frame_timer;
        u32              frames_per_second_counter;
        u32              frames_per_second;
        f64              last_time;
        f32              delta_time;

        bool             is_minimized;
        bool             is_running;
        bool             is_inventory_active;
        bool             is_cursor_locked;

        bool             is_visual_debugging_enabled;
        Game_Debug_State debug_state;
    };

    bool initialize_game(Game_State *game_state);
    void shutdown_game(Game_State *game_state);

    void run_game(Game_State *game_state);

    void toggle_visual_debugging(Game_State *game_state);
    void toggle_inventory(Game_State *game_state);

    bool game_on_quit(const Event *event, void *sender);
    bool game_on_key_press(const Event *event, void *sender);
    bool game_on_mouse_press(const Event *event, void *sender);
    bool game_on_mouse_wheel(const Event *event, void *sender);
    bool game_on_mouse_move(const Event *event, void *sender);
    bool game_on_char(const Event *event, void *sender);
    bool game_on_resize(const Event *event, void *sender);
    bool game_on_minimize(const Event *event, void *sender);
    bool game_on_restore(const Event *event, void *sender);
}