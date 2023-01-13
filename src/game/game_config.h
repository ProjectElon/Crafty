#pragma once

#include "core/common.h"

namespace minecraft {

    enum WindowMode : u8
    {
        WindowMode_None,
        WindowMode_Fullscreen,
        WindowMode_BorderlessFullscreen,
        WindowMode_Windowed
    };

    struct Game_Config
    {
        char       window_title[256];
        i32        window_x;
        i32        window_y;
        i32        window_x_before_fullscreen;
        i32        window_y_before_fullscreen;
        u32        window_width;
        u32        window_height;
        WindowMode window_mode;
        bool       is_cursor_visible;
        bool       is_raw_mouse_motion_enabled;
        bool       is_fxaa_enabled;
        u32        chunk_radius;
    };

    void load_game_config_defaults(Game_Config *config);

    bool load_game_config(Game_Config *config,
                          const char *config_file_path);

    bool save_game_config(Game_Config *config,
                          const char *config_file_path);
}
