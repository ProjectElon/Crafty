#pragma once

#include "core/common.h"
#include "input_codes.h"

#include <glm/glm.hpp>

struct GLFWwindow;

namespace minecraft {

    #define MC_KEY_STATE_COUNT    512
    #define MC_BUTTON_STATE_COUNT 64

    struct InputData
    {
        glm::vec2   previous_mouse_position;
        glm::vec2   mouse_position;
        bool        previous_key_states[MC_KEY_STATE_COUNT];
        bool        key_states[MC_KEY_STATE_COUNT];
        bool        previous_button_states[MC_BUTTON_STATE_COUNT];
        bool        button_states[MC_BUTTON_STATE_COUNT];
        bool        is_cursor_visible;
        bool        is_using_raw_mouse_motion;
    };

    struct Input
    {
        static InputData internal_data;

        static bool initialize(GLFWwindow *window);
        static void shutdown();

        static void update(GLFWwindow *window);

        static void set_cursor_mode(GLFWwindow *window, bool enabled);
        static void set_raw_mouse_motion(GLFWwindow *window, bool enabled);
        static void toggle_cursor(GLFWwindow *window);

        static bool get_key(u16 key_code);

        static bool is_key_pressed(u16 key_code);
        static bool is_key_held(u16 key_code);
        static bool is_key_released(u16 key_code);

        static bool get_button(u8 button_code);

        static bool is_button_pressed(u8 button_code);
        static bool is_button_held(u8 button_code);
        static bool is_button_released(u8 button_code);

        static glm::vec2 get_mouse_position();
        static glm::vec2 get_mouse_delta();

        static const char *convert_key_code_to_string(u16 key_code);
        static const char *convert_button_code_to_string(u16 button_code);
    };
}