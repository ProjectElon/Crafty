#pragma once

#include "core/common.h"

namespace minecraft {

    enum WindowMode
    {
        WindowMode_Fullscreen = 0,
        WindowMode_BorderlessFullscreen = 1,
        WindowMode_Windowed = 2
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

    struct Game_Data
    {
        Game_Config config;
        struct Platform *platform;
        struct Camera   *camera;

        bool is_running;
        bool should_update_camera;
        bool show_debug_stats_hud;
    };

    struct Game
    {
        static Game_Data internal_data;

        Game()  = delete;
        ~Game() = delete;

        static bool start();
        static void shutdown();

        static inline Game_Config& get_config() { return internal_data.config; }
        static inline Platform& get_platform() { return *internal_data.platform; }
        static inline Camera& get_camera() { return *internal_data.camera; }

        static inline bool is_running() { return internal_data.is_running; }
        static inline bool should_update_camera() { return internal_data.should_update_camera; }
        static inline bool show_debug_status_hud() { return internal_data.show_debug_stats_hud; }

        static inline void toggle_should_update_camera()
        {
            internal_data.should_update_camera = !internal_data.should_update_camera;
        }

        static inline void toggle_show_debug_status_hud()
        {
            internal_data.show_debug_stats_hud = !internal_data.show_debug_stats_hud;
        }
    };
}