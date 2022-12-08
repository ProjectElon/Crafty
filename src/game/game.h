#pragma once

#include "core/common.h"
#include "renderer/camera.h"

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
        bool        minimized;
    };

    struct Game_State
    {
        Game_Config  config;
        GLFWwindow  *window;

        Camera      camera;

        bool        is_running;
        bool        show_debug_stats_hud;
        bool        is_inventory_active;

        CursorMode  cursor_mode;

        void *ingame_crosshair_texture;
        void *inventory_crosshair_texture;
    };

    struct Game
    {
        static Game_State internal_data;

        static bool initialize();
        static void shutdown();

        static void run();

        static inline Game_Config& get_config() { return internal_data.config; }
        static inline Game_State& get_state() { return internal_data; }
        static inline Camera& get_camera() { return internal_data.camera; }

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