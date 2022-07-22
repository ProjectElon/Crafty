#pragma once

#include "core/common.h"

namespace minecraft {

    enum WindowMode
    {
        WindowMode_Fullscreen = 0,
        WindowMode_BorderlessFullscreen = 1,
        WindowMode_Windowed = 2
    };

    enum CursorMode
    {
        CursorMode_Locked,
        CursorMode_Free
    };

    struct Game_Config
    {
        const char *window_title = "Minecraft";
        i32 window_x = -1;
        i32 window_y = -1;
        u32 window_width = 1280;
        u32 window_height = 720;
        WindowMode window_mode = WindowMode_Windowed;
        bool minimized = false;
    };

    struct Platform;
    struct Camera;

    struct Game_Data
    {
        Game_Config config;
        Platform *platform;
        Camera   *camera;

        bool is_running;
        bool show_debug_stats_hud;
        bool is_inventory_active;

        CursorMode cursor_mode;
    };

    struct Game
    {
        static Game_Data internal_data;

        Game()  = delete;
        ~Game() = delete;

        static bool initialize();
        static void shutdown();

        static inline Game_Config& get_config() { return internal_data.config; }
        static inline Platform& get_platform() { return *internal_data.platform; }
        static inline Camera& get_camera() { return *internal_data.camera; }

        static inline bool is_running() { return internal_data.is_running; }
        static inline bool show_debug_status_hud() { return internal_data.show_debug_stats_hud; }
        static inline bool should_render_inventory() { return internal_data.is_inventory_active; }

        static inline void toggle_show_debug_status_hud()
        {
            internal_data.show_debug_stats_hud = !internal_data.show_debug_stats_hud;
        }

        static inline void toggle_inventory()
        {
            internal_data.is_inventory_active = !internal_data.is_inventory_active;
        }

        static inline void set_cursor_mode(CursorMode mode)
        {
            internal_data.cursor_mode = mode;
        }
    };
}