#include "input.h"

#include "platform.h"
#include <GLFW/glfw3.h>

namespace minecraft {

    bool Input::initialize(Platform *platform)
    {
        glfwSetInputMode(platform->window_handle, GLFW_LOCK_KEY_MODS, GLFW_TRUE);
        internal_data.window_handle = platform->window_handle;
        memset(internal_data.key_states, 0, sizeof(bool) * MC_KEY_STATE_COUNT);
        memset(internal_data.previous_key_states, 0, sizeof(bool) * MC_KEY_STATE_COUNT);
        memset(internal_data.button_states, 0, sizeof(bool) * MC_BUTTON_STATE_COUNT);
        memset(internal_data.previous_button_states, 0, sizeof(bool) * MC_BUTTON_STATE_COUNT);
        internal_data.mouse_position = { 0.0f, 0.0f };
        internal_data.previous_mouse_position = { 0.0f, 0.0f };
        return true;
    }

    void Input::shutdown()
    {
    }

    void Input::update()
    {
        memcpy(internal_data.previous_key_states, internal_data.key_states, sizeof(bool) * MC_KEY_STATE_COUNT);
        memcpy(internal_data.previous_button_states, internal_data.button_states, sizeof(bool) * MC_BUTTON_STATE_COUNT);

        for (u32 key_code = 0; key_code < MC_KEY_STATE_COUNT; key_code++)
        {
            internal_data.key_states[key_code] = glfwGetKey(internal_data.window_handle, key_code) == GLFW_PRESS;
        }

        for (u32 button_code = 0; button_code < MC_BUTTON_STATE_COUNT; button_code++)
        {
            internal_data.button_states[button_code] = glfwGetMouseButton(internal_data.window_handle, button_code) == GLFW_PRESS;
        }

        f64 mouse_x, mouse_y;
        glfwGetCursorPos(internal_data.window_handle, &mouse_x, &mouse_y);

        internal_data.previous_mouse_position = internal_data.mouse_position;
        internal_data.mouse_position = { (f32)mouse_x, (f32)mouse_y };
    }

    void Input::set_cursor_mode(bool enabled)
    {
        glfwSetInputMode(internal_data.window_handle, GLFW_CURSOR, enabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
        internal_data.is_cursor_visible = enabled;
    }

    void Input::set_raw_mouse_motion(bool enabled)
    {
        if (glfwRawMouseMotionSupported())
        {
            glfwSetInputMode(internal_data.window_handle, GLFW_RAW_MOUSE_MOTION, enabled ? GLFW_TRUE : GLFW_FALSE);
            internal_data.is_using_raw_mouse_motion = enabled;
        }
    }

    void Input::toggle_cursor()
    {
        if (Input::internal_data.is_cursor_visible)
        {
            Input::set_cursor_mode(false);
            Input::set_raw_mouse_motion(true);
        }
        else
        {
            Input::set_cursor_mode(true);
            Input::set_raw_mouse_motion(false);
        }
    }

    bool Input::get_key(u16 key_code)
    {
        return internal_data.key_states[key_code];
    }

    bool Input::is_key_pressed(u16 key_code)
    {
        return internal_data.key_states[key_code] && !internal_data.previous_key_states[key_code];
    }

    bool Input::is_key_held(u16 key_code)
    {
        return internal_data.key_states[key_code] && internal_data.previous_key_states[key_code];
    }

    bool Input::is_key_released(u16 key_code)
    {
        return !internal_data.key_states[key_code] && internal_data.previous_key_states[key_code];
    }

    bool Input::get_button(u8 button_code)
    {
        return internal_data.button_states[button_code];
    }

    bool Input::is_button_pressed(u8 button_code)
    {
        return internal_data.button_states[button_code] && !internal_data.previous_button_states[button_code];
    }

    bool Input::is_button_held(u8 button_code)
    {
        return internal_data.button_states[button_code] && internal_data.previous_button_states[button_code];
    }

    bool Input::is_button_released(u8 button_code)
    {
        return !internal_data.button_states[button_code] && internal_data.previous_button_states[button_code];
    }

    glm::vec2 Input::get_mouse_position()
    {
        return internal_data.mouse_position;
    }

    glm::vec2 Input::get_mouse_delta()
    {
        return internal_data.mouse_position - internal_data.previous_mouse_position;
    }

    const char* Input::convert_key_code_to_string(u16 key_code)
    {
        switch (key_code)
        {
            case MC_KEY_LEFT_SHIFT: return "left shift"; break;
            case MC_KEY_RIGHT_SHIFT: return "right shift"; break;
            case MC_KEY_LEFT_ALT: return "left alt"; break;
            case MC_KEY_RIGHT_ALT: return "right alt"; break;
            case MC_KEY_LEFT_CONTROL: return "left control"; break;
            case MC_KEY_RIGHT_CONTROL: return "right control"; break;
            case MC_KEY_SPACE: return "space"; break;
            case MC_KEY_BACKSPACE: return "back space"; break;
            case MC_KEY_LEFT_SUPER: return "left super"; break;
            case MC_KEY_RIGHT_SUPER: return "right super"; break;
            case MC_KEY_ENTER: return "enter"; break;
            case MC_KEY_TAB: return "tab"; break;
            case MC_KEY_CAPS_LOCK: return "capslock"; break;
        }

        return glfwGetKeyName(key_code, 0);
    }

    const char* Input::convert_button_code_to_string(u16 button_code)
    {
        switch (button_code)
        {
            case MC_MOUSE_BUTTON_LEFT: return "left"; break;
            case MC_MOUSE_BUTTON_MIDDLE: return "middle"; break;
            case MC_MOUSE_BUTTON_RIGHT: return "right"; break;
        }
        return "";
    }

    InputData Input::internal_data;
}