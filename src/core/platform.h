#pragma once

#include "common.h"
#include "game/game.h"

struct GLFWwindow;

namespace minecraft {

    struct Platform
    {
        static bool initialize(Game_Config *config,
                               u32          opengl_major_version,
                               u32          opengl_minor_version);

        static void shutdown();

        static bool opengl_initialize(GLFWwindow *window);
        static void opengl_swap_buffers(GLFWwindow *window);

        static GLFWwindow *open_window(const char *title,
                                       u32 width,
                                       u32 height,
                                       u32 back_buffer_samples);

        static void set_window_user_pointer(GLFWwindow *window, void *user_pointer);
        static void* get_window_user_pointer(GLFWwindow *window);

        static void hook_window_event_callbacks(GLFWwindow *window);

        static void pump_messages();

        static void switch_to_window_mode(GLFWwindow  *window,
                                          Game_Config *config,
                                          WindowMode   new_window_mode);

        static void center_window(GLFWwindow *window, Game_Config *config);

        static void set_cursor_visiblity(GLFWwindow *window, Game_Config *config, bool is_visible);
        static void set_raw_mouse_motion(GLFWwindow *window, Game_Config *config, bool is_enabled);
        static void toggle_cursor_visiblity(GLFWwindow *window, Game_Config *config);

        static f64 get_current_time_in_seconds();
    };
}