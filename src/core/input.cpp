#include "input.h"

#include "platform.h"
#include "ui/dropdown_console.h"
#include <GLFW/glfw3.h>

namespace minecraft {

    bool initialize_input(Input *input, GLFWwindow *window)
    {
        glfwSetInputMode(window, GLFW_LOCK_KEY_MODS, GLFW_TRUE);

        memset(input->key_states,             0, sizeof(bool) * MC_KEY_STATE_COUNT);
        memset(input->previous_key_states,    0, sizeof(bool) * MC_KEY_STATE_COUNT);
        memset(input->button_states,          0, sizeof(bool) * MC_BUTTON_STATE_COUNT);
        memset(input->previous_button_states, 0, sizeof(bool) * MC_BUTTON_STATE_COUNT);

        input->previous_mouse_position   = { 0.0f, 0.0f };
        input->mouse_position            = { 0.0f, 0.0f };
        input->is_cursor_visible         = true;
        input->is_using_raw_mouse_motion = false;

        return true;
    }

    void shutdown_input(Input *input)
    {
    }

    void update_input(Input *input, GLFWwindow *window)
    {
        memcpy(input->previous_key_states,    input->key_states,    sizeof(bool) * MC_KEY_STATE_COUNT);
        memcpy(input->previous_button_states, input->button_states, sizeof(bool) * MC_BUTTON_STATE_COUNT);

        for (u32 key_code = 0; key_code < MC_KEY_STATE_COUNT; key_code++)
        {
            input->key_states[key_code] =
                glfwGetKey(window, key_code) == GLFW_PRESS;
        }

        for (u32 button_code = 0; button_code < MC_BUTTON_STATE_COUNT; button_code++)
        {
            input->button_states[button_code] =
                glfwGetMouseButton(window, button_code) == GLFW_PRESS;
        }

        f64 mouse_x, mouse_y;
        glfwGetCursorPos(window, &mouse_x, &mouse_y);

        input->previous_mouse_position = input->mouse_position;
        input->mouse_position          = { (f32)mouse_x, (f32)mouse_y };
    }

    bool get_key(Input *input, u16 key_code)
    {
        return input->key_states[key_code];
    }

    bool is_key_pressed(Input *input, u16 key_code)
    {
        return input->key_states[key_code] && !input->previous_key_states[key_code];
    }

    bool is_key_held(Input *input, u16 key_code)
    {
        return input->key_states[key_code] && input->previous_key_states[key_code];
    }

    bool is_key_released(Input *input, u16 key_code)
    {
        return !input->key_states[key_code] && input->previous_key_states[key_code];
    }

    bool get_button(Input *input, u8 button_code)
    {
        return input->button_states[button_code];
    }

    bool is_button_pressed(Input *input, u8 button_code)
    {
        return input->button_states[button_code] &&
              !input->previous_button_states[button_code];
    }

    bool is_button_held(Input *input, u8 button_code)
    {
        return input->button_states[button_code] &&
               input->previous_button_states[button_code];
    }

    bool is_button_released(Input *input, u8 button_code)
    {
        return !input->button_states[button_code] &&
                input->previous_button_states[button_code];
    }

    glm::vec2 get_mouse_delta(Input *input)
    {
        return input->mouse_position - input->previous_mouse_position;
    }

    const char* convert_key_code_to_string(u16 key_code)
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

    const char* convert_button_code_to_string(u16 button_code)
    {
        switch (button_code)
        {
            case MC_MOUSE_BUTTON_LEFT:   return "left";  break;
            case MC_MOUSE_BUTTON_MIDDLE: return "middle"; break;
            case MC_MOUSE_BUTTON_RIGHT:  return "right";  break;
        }
        return "";
    }
}