#pragma once

#include "core/common.h"
#include "core/event.h"
#include "core/input.h"

#include "memory/memory_arena.h"

#include "containers/string.h"

#include "renderer/camera.h"

#include "game/world.h"

struct GLFWwindow;

namespace minecraft {

    enum WindowMode
    {
        WindowMode_Fullscreen           = 0,
        WindowMode_BorderlessFullscreen = 1,
        WindowMode_Windowed             = 2
    };

    enum CursorMode
    {
        CursorMode_Locked,
        CursorMode_Free
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

    struct Game_State
    {
        Game_Memory *game_memory;
        Game_Config  game_config;
        Event_System event_system;
        Input        game_input;
        World        world;
        GLFWwindow  *window;
        Camera       camera;
        bool         minimized;
        bool         is_running;
        bool         show_debug_stats_hud;
        bool         is_inventory_active;
        CursorMode   cursor_mode;

        // todo(harlequin): game_assets
        void        *ingame_crosshair_texture;
        void        *inventory_crosshair_texture;
    };

    bool initialize_game(Game_State *game_state);
    void shutdown_game(Game_State *game_state);
    void run_game(Game_State *game_state);

    void toggle_show_debug_status_hud(Game_State *game_state);
    void toggle_inventory(Game_State *game_state);
    void set_cursor_mode(Game_State *game_state, CursorMode mode);

    String8 push_formatted_string8(Memory_Arena *arena, const char *format, ...);
}