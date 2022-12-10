#include "event.h"

#include "platform.h"
#include "input.h"

namespace minecraft {

    static const char* convert_event_type_to_string(EventType event_type)
    {
        switch (event_type)
        {
            // Window Events
            case EventType_Resize:   { return "Resize";   break; }
            case EventType_Minimize: { return "Minimize"; break; }
            case EventType_Restore:  { return "Restore";  break; }
            case EventType_Quit:     { return "Quit";     break; }

            // Key Events
            case EventType_KeyPress:   return "KeyPress";   break;
            case EventType_KeyHeld:    return "KeyHeld";    break;
            case EventType_KeyRelease: return "KeyRelease"; break;
            case EventType_Char:       return "Char";       break;

            // Mouse Events
            case EventType_MouseButtonPress:   return "MouseButtonPress";   break;
            case EventType_MouseButtonHeld:    return "MouseButtonHeld";    break;
            case EventType_MouseButtonRelease: return "MouseButtonRelease"; break;
            case EventType_MouseWheel:         return "MouseWheel";         break;
            case EventType_MouseMove:          return "MouseMove";          break;
        }

        return "";
    }

    static const char* convert_event_to_string(EventType event_type, const Event *event)
    {
        static char internal_string_buffer[1024];

        switch (event_type)
        {
            case EventType_Resize:
            {
                return "[EVENT]: Resize";
            } break;

            case EventType_Minimize:
            {
                return "[EVENT]: Minimize";
            } break;

            case EventType_Restore:
            {
                return "[EVENT]: Restore";
            } break;

            case EventType_Quit:
            {
                return "[EVENT]: Quit";
            } break;

            // Key Events
            case EventType_KeyPress:
            {
                u16 key = event->data_u16;
                sprintf(internal_string_buffer,
                        "[EVENT]: KeyPress => key: \"%s\"",
                        Input::convert_key_code_to_string(key));
                return internal_string_buffer;
            } break;

            case EventType_KeyHeld:
            {
                u16 key = event->data_u16;
                sprintf(internal_string_buffer,
                        "[EVENT]: KeyHeld => key: \"%s\"",
                        Input::convert_key_code_to_string(key));
                return internal_string_buffer;
            } break;

            case EventType_KeyRelease:
            {
                u16 key = event->data_u16;
                sprintf(internal_string_buffer,
                        "[EVENT]: KeyRelease => key: \"%s\"",
                        Input::convert_key_code_to_string(key));
                return internal_string_buffer;
            } break;

            case EventType_Char:
            {
                u8 code_point = event->data_u8;
                sprintf(internal_string_buffer,
                        "[EVENT]: Char => key: \"%c\"",
                        (unsigned char)code_point);
                return internal_string_buffer;
            } break;

            // Mouse Events
            case EventType_MouseButtonPress:
            {
                u16 button = event->data_u16;
                sprintf(internal_string_buffer,
                        "[EVENT]: MouseButtonPress => button: \"%s\"",
                        Input::convert_button_code_to_string(button));
                return internal_string_buffer;
            } break;

            case EventType_MouseButtonHeld:
            {
                u16 button = event->data_u16;
                sprintf(internal_string_buffer,
                        "[EVENT]: MouseButtonHeld => button: \"%s\"",
                        Input::convert_button_code_to_string(button));
                return internal_string_buffer;
            } break;

            case EventType_MouseButtonRelease:
            {
                u16 button = event->data_u16;
                sprintf(internal_string_buffer,
                        "[EVENT]: MouseButtonRelease => button: \"%s\"",
                        Input::convert_button_code_to_string(button));
                return internal_string_buffer;
            } break;

            case EventType_MouseWheel:
            {
                f32 xoffset = event->data_f32_array[0];
                f32 yoffset = event->data_f32_array[1];
                const char *direction;
                if (yoffset > 0.0f) direction = "up";
                if (yoffset < 0.0f) direction = "down";
                sprintf(internal_string_buffer,
                        "[EVENT]: MouseWheel => direction: \"%s\"",
                        direction);
                return internal_string_buffer;
            } break;

            case EventType_MouseMove:
            {
                f32 mouse_x = event->data_f32_array[0];
                f32 mouse_y = event->data_f32_array[1];
                sprintf(internal_string_buffer,
                        "[EVENT]: MouseMouse => position: \"(%f, %f)\"",
                        mouse_x, mouse_y);
                return internal_string_buffer;
            } break;
        }
    }

    bool Event_System::initialize(bool is_logging_enabled)
    {
        internal_data.is_logging_enabled = is_logging_enabled;
        internal_data.event_registry->entries.reserve(1024);
        return true;
    }

    void Event_System::shutdown()
    {
    }

    bool Event_System::register_event(EventType event_type, on_event_fn on_event, void *sender)
    {
        for (auto& event_entry : internal_data.event_registry[event_type].entries)
        {
            if (event_entry.sender == sender && event_entry.on_event == on_event)
            {
                fprintf(stderr, "[WARNING]: %s event is already registered with sender %p\n", convert_event_type_to_string(event_type), sender);
                return false;
            }
        }

        Event_Entry event_entry;
        event_entry.on_event = on_event;
        event_entry.sender   = sender;
        internal_data.event_registry[event_type].entries.emplace_back(event_entry);
        fprintf(stderr, "[TRACE]: %s event registered with sender %p\n", convert_event_type_to_string(event_type), sender);
        return true;
    }

    bool Event_System::unregister_event(EventType event_type, void *sender)
    {
        auto& entries = internal_data.event_registry[event_type].entries;

        u32 count = entries.size();

        for (u32 i = 0; i < count; i++)
        {
            if (entries[i].sender == sender)
            {
                entries.erase(entries.begin() + i);
                fprintf(stderr, "[TRACE]: %s event unregistered with sender %p\n", convert_event_type_to_string(event_type), sender);
                return true;
            }
        }

        fprintf(stderr, "[WARNING]: %s event is not registered with sender %p\n", convert_event_type_to_string(event_type), sender);
        return false;
    }

    void Event_System::fire_event(EventType event_type, const Event *event)
    {
        for (auto& event_entry : internal_data.event_registry[event_type].entries)
        {
            bool handled = event_entry.on_event(event, event_entry.sender);

            if (internal_data.is_logging_enabled)
            {
                const char* event_string = convert_event_to_string(event_type, event);
                fprintf(stderr, "[TRACE]: %s event fired with sender %p\n", event_string, event_entry.sender);
            }

            if (handled)
            {
                break;
            }
        }
    }

    void Event_System::parse_resize_event(const Event *event, u32 *out_width, u32 *out_height)
    {
        *out_width  = event->data_u32_array[0];
        *out_height = event->data_u32_array[1];
    }

    void Event_System::parse_key_code(const Event *event, u16 *out_key)
    {
        *out_key = event->data_u16;
    }
    void Event_System::parse_mouse_move(const Event *event, f32 *out_mouse_x, f32 *out_mouse_y)
    {
        *out_mouse_x = event->data_f32_array[0];
        *out_mouse_y = event->data_f32_array[1];
    }
    void Event_System::parse_button_code(const Event *event, u8 *out_button)
    {
        *out_button = event->data_u8;
    }
    void Event_System::parse_model_wheel(const Event *event, f32 *out_xoffset, f32 *out_yoffset)
    {
        *out_xoffset = event->data_f32_array[0];
        *out_yoffset = event->data_f32_array[1];
    }

    void Event_System::parse_char(const Event *event, char *out_code_point)
    {
        *out_code_point = event->data_u8;
    }

    Event_System_Data Event_System::internal_data;
}