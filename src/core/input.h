#pragma once

#include "core/common.h"
#include "input_codes.h"

#include <glm/glm.hpp>

struct GLFWwindow;

namespace minecraft {

    #define MC_KEY_STATE_COUNT    512
    #define MC_BUTTON_STATE_COUNT 64

    struct Input
    {
        glm::vec2 previous_mouse_position;
        glm::vec2 mouse_position;
        bool      previous_key_states[MC_KEY_STATE_COUNT];
        bool      key_states[MC_KEY_STATE_COUNT];
        bool      previous_button_states[MC_BUTTON_STATE_COUNT];
        bool      button_states[MC_BUTTON_STATE_COUNT];
        bool      is_cursor_visible;
        bool      is_using_raw_mouse_motion;
    };

    bool initialize_input(Input *input, GLFWwindow *window);
    void shutdown_input(Input *input);

    void update_input(Input *input, GLFWwindow *window);

    bool get_key(Input *input, u16 key_code);

    bool is_key_pressed(Input *input, u16 key_code);
    bool is_key_held(Input *input, u16 key_code);
    bool is_key_released(Input *input, u16 key_code);

    bool get_button(Input *input, u8 button_code);

    bool is_button_pressed(Input *input, u8 button_code);
    bool is_button_held(Input *input, u8 button_code);
    bool is_button_released(Input *input, u8 button_code);

    glm::vec2 get_mouse_position(Input *input);
    glm::vec2 get_mouse_delta(Input *input);

    const char *convert_key_code_to_string(u16 key_code);
    const char *convert_button_code_to_string(u16 button_code);

}