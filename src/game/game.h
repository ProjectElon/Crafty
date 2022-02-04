#pragma once

#include "core/common.h"

namespace minecraft {

    enum WindowMode
    {
        WindowMode_Fullscreen = 0,
        WindowMode_BorderlessFullscreen = 1,
        WindowMode_Windowed = 2
    };

    // set window_x, window_y to -1 to center the window
    struct Game_Config
    {
        const char *window_title = "Minecraft";
        i32 window_x = -1;
        i32 window_y = -1;
        u32 window_width = 1280;
        u32 window_height = 720;
        WindowMode window_mode = WindowMode_Windowed;

        bool update_camera = true;
    };

    struct Game
    {
        Game_Config config = {};
        bool is_running = false;
    };
}