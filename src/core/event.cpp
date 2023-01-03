#include "event.h"

#include "platform.h"
#include "input.h"

namespace minecraft {

    bool initialize_event_system(Event_System *event_system, bool is_logging_enabled)
    {
        event_system->is_logging_enabled = is_logging_enabled;
        event_system->registry->entries  = std::vector< Event_Entry >(); // todo(harlequin): remove std::vector
        event_system->registry->entries.reserve(128);
        return true;
    }

    void shutdown_event_system(Event_System *event_system)
    {
    }

    bool register_event(Event_System *event_system,
                        EventType event_type,
                        on_event_fn on_event,
                        void *sender)
    {
        for (auto& event_entry : event_system->registry[event_type].entries)
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
        event_system->registry[event_type].entries.emplace_back(event_entry);
        fprintf(stderr, "[TRACE]: %s event registered with sender %p\n", convert_event_type_to_string(event_type), sender);
        return true;
    }

    bool unregister_event(Event_System *event_system, EventType event_type, void *sender)
    {
        auto& entries = event_system->registry[event_type].entries;

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

    void fire_event(Event_System *event_system, EventType event_type, const Event *event)
    {
        i32 count = event_system->registry[event_type].entries.size();
        for (i32 i = count - 1; i >= 0; i--)
        {
            Event_Entry &event_entry = event_system->registry[event_type].entries[i];
            bool handled             = event_entry.on_event(event, event_entry.sender);

            if (event_system->is_logging_enabled)
            {
                const char* event_string = convert_event_to_string(event_type, event);
                fprintf(stderr,
                        "[TRACE]: %s event fired with sender %p\n",
                        event_string,
                        event_entry.sender);
            }

            if (handled)
            {
                break;
            }
        }
    }

    void parse_resize_event(const Event *event, u32 *out_width, u32 *out_height)
    {
        *out_width  = event->data_u32_array[0];
        *out_height = event->data_u32_array[1];
    }

    void parse_key_code(const Event *event, u16 *out_key)
    {
        *out_key = event->data_u16;
    }
    void parse_mouse_move(const Event *event, f32 *out_mouse_x, f32 *out_mouse_y)
    {
        *out_mouse_x = event->data_f32_array[0];
        *out_mouse_y = event->data_f32_array[1];
    }
    void parse_button_code(const Event *event, u8 *out_button)
    {
        *out_button = event->data_u8;
    }
    void parse_mouse_wheel(const Event *event, f32 *out_xoffset, f32 *out_yoffset)
    {
        *out_xoffset = event->data_f32_array[0];
        *out_yoffset = event->data_f32_array[1];
    }

    void parse_char(const Event *event, char *out_code_point)
    {
        *out_code_point = event->data_u8;
    }

    const char* convert_event_type_to_string(EventType event_type)
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

    const char* convert_event_to_string(EventType event_type, const Event *event)
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
                        convert_key_code_to_string(key));
                return internal_string_buffer;
            } break;

            case EventType_KeyHeld:
            {
                u16 key = event->data_u16;
                sprintf(internal_string_buffer,
                        "[EVENT]: KeyHeld => key: \"%s\"",
                        convert_key_code_to_string(key));
                return internal_string_buffer;
            } break;

            case EventType_KeyRelease:
            {
                u16 key = event->data_u16;
                sprintf(internal_string_buffer,
                        "[EVENT]: KeyRelease => key: \"%s\"",
                        convert_key_code_to_string(key));
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
                        convert_button_code_to_string(button));
                return internal_string_buffer;
            } break;

            case EventType_MouseButtonHeld:
            {
                u16 button = event->data_u16;
                sprintf(internal_string_buffer,
                        "[EVENT]: MouseButtonHeld => button: \"%s\"",
                        convert_button_code_to_string(button));
                return internal_string_buffer;
            } break;

            case EventType_MouseButtonRelease:
            {
                u16 button = event->data_u16;
                sprintf(internal_string_buffer,
                        "[EVENT]: MouseButtonRelease => button: \"%s\"",
                        convert_button_code_to_string(button));
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
}