#pragma once

#include "common.h"
#include "game/game.h"

struct GLFWwindow;

namespace minecraft {

    struct Platform
    {
        // note(harlequin): we are going to have one main window throughout the game
        Game *game;
        GLFWwindow *window_handle;
        i32 window_x_before_fullscreen;
        i32 window_y_before_fullscreen;

        bool initialize(Game *game,
                        u32 opengl_major_version = 4,
                        u32 opengl_minor_version = 5);

        bool opengl_initialize();
        void opengl_swap_buffers();

        void pump_messages();

        void switch_to_window_mode(WindowMode new_window_mode);
        void center_window();

        f32 get_current_time();

        void shutdown();
    };
}