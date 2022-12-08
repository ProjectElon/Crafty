#include "input.h"

#include "platform.h"
#include "ui/dropdown_console.h"
#include <GLFW/glfw3.h>

namespace minecraft {

    bool Input::initialize(GLFWwindow *window)
    {
        auto& id = internal_data;
        glfwSetInputMode(window, GLFW_LOCK_KEY_MODS, GLFW_TRUE);

        memset(id.key_states,             0, sizeof(bool) * MC_KEY_STATE_COUNT);
        memset(id.previous_key_states,    0, sizeof(bool) * MC_KEY_STATE_COUNT);
        memset(id.button_states,          0, sizeof(bool) * MC_BUTTON_STATE_COUNT);
        memset(id.previous_button_states, 0, sizeof(bool) * MC_BUTTON_STATE_COUNT);

        id.previous_mouse_position   = { 0.0f, 0.0f };
        id.mouse_position            = { 0.0f, 0.0f };
        id.is_cursor_visible         = true;
        id.is_using_raw_mouse_motion = false;

        return true;
    }

    void Input::shutdown()
    {
    }

    void Input::update(GLFWwindow *window)
    {
        auto& id = internal_data;

        memcpy(id.previous_key_states,    id.key_states,    sizeof(bool) * MC_KEY_STATE_COUNT);
        memcpy(id.previous_button_states, id.button_states, sizeof(bool) * MC_BUTTON_STATE_COUNT);

        for (u32 key_code = 0; key_code < MC_KEY_STATE_COUNT; key_code++)
        {
            // todo(harlequin): why are we only allowing input when the console is closed here
            id.key_states[key_code] =
                Dropdown_Console::is_closed() &&
                glfwGetKey(window, key_code) == GLFW_PRESS;
        }

        for (u32 button_code = 0; button_code < MC_BUTTON_STATE_COUNT; button_code++)
        {
            // todo(harlequin): why are we only allowing input when the console is closed here
            id.button_states[button_code] =
                Dropdown_Console::is_closed() &&
                glfwGetMouseButton(window, button_code) == GLFW_PRESS;
        }

        f64 mouse_x, mouse_y;
        glfwGetCursorPos(window, &mouse_x, &mouse_y);

        id.previous_mouse_position = id.mouse_position;
        id.mouse_position          = { (f32)mouse_x, (f32)mouse_y };
    }

    void Input::set_cursor_mode(GLFWwindow *window, bool enabled)
    {
        auto& id = internal_data;

        glfwSetInputMode(window,
                         GLFW_CURSOR, enabled ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_DISABLED);
        id.is_cursor_visible = enabled;
    }

    void Input::set_raw_mouse_motion(GLFWwindow *window, bool enabled)
    {
        auto& id = internal_data;

        if (glfwRawMouseMotionSupported())
        {
            glfwSetInputMode(window,
                             GLFW_RAW_MOUSE_MOTION, enabled ? GLFW_TRUE : GLFW_FALSE);
            id.is_using_raw_mouse_motion = enabled;
        }
    }

    void Input::toggle_cursor(GLFWwindow *window)
    {
        auto& id = internal_data;

        if (id.is_cursor_visible)
        {
            set_cursor_mode(window, false);
        }
        else
        {
            set_cursor_mode(window, true);
        }
    }

    bool Input::get_key(u16 key_code)
    {
        auto& id = internal_data;
        return id.key_states[key_code];
    }

    bool Input::is_key_pressed(u16 key_code)
    {
        auto& id = internal_data;
        return id.key_states[key_code] && !id.previous_key_states[key_code];
    }

    bool Input::is_key_held(u16 key_code)
    {
        auto& id = internal_data;
        return id.key_states[key_code] && id.previous_key_states[key_code];
    }

    bool Input::is_key_released(u16 key_code)
    {
        auto& id = internal_data;
        return !id.key_states[key_code] && id.previous_key_states[key_code];
    }

    bool Input::get_button(u8 button_code)
    {
        auto& id = internal_data;
        return id.button_states[button_code];
    }

    bool Input::is_button_pressed(u8 button_code)
    {
        auto& id = internal_data;
        return id.button_states[button_code] &&
              !id.previous_button_states[button_code];
    }

    bool Input::is_button_held(u8 button_code)
    {
        auto& id = internal_data;
        return id.button_states[button_code] &&
               id.previous_button_states[button_code];
    }

    bool Input::is_button_released(u8 button_code)
    {
        auto& id = internal_data;
        return !id.button_states[button_code] &&
                id.previous_button_states[button_code];
    }

    glm::vec2 Input::get_mouse_position()
    {
        auto& id = internal_data;
        return id.mouse_position;
    }

    glm::vec2 Input::get_mouse_delta()
    {
        auto& id = internal_data;
        return id.mouse_position - id.previous_mouse_position;
    }

    const char* Input::convert_key_code_to_string(u16 key_code)
    {
        switch (key_code)
        {
            case MC_KEY_LEFT_SHIFT:    return "left shift";    break;
            case MC_KEY_RIGHT_SHIFT:   return "right shift";   break;
            case MC_KEY_LEFT_ALT:      return "left alt";      break;
            case MC_KEY_RIGHT_ALT:     return "right alt";     break;
            case MC_KEY_LEFT_CONTROL:  return "left control";  break;
            case MC_KEY_RIGHT_CONTROL: return "right control"; break;
            case MC_KEY_SPACE:         return "space";         break;
            case MC_KEY_BACKSPACE:     return "back space";    break;
            case MC_KEY_LEFT_SUPER:    return "left super";    break;
            case MC_KEY_RIGHT_SUPER:   return "right super";   break;
            case MC_KEY_ENTER:         return "enter";         break;
            case MC_KEY_TAB:           return "tab";           break;
            case MC_KEY_CAPS_LOCK:     return "capslock";      break;
        }

        return glfwGetKeyName(key_code, 0);
    }

    const char* Input::convert_button_code_to_string(u16 button_code)
    {
        switch (button_code)
        {
            case MC_MOUSE_BUTTON_LEFT:   return "left";  break;
            case MC_MOUSE_BUTTON_MIDDLE: return "middle"; break;
            case MC_MOUSE_BUTTON_RIGHT:  return "right";  break;
        }
        return "";
    }

    InputData Input::internal_data;
}