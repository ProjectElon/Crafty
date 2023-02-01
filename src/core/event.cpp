#include "event.h"

#include "platform.h"
#include "memory/memory_arena.h"
#include "input.h"

namespace minecraft {

    bool initialize_event_system(Event_System *event_system,
                                 Memory_Arena *arena,
                                 bool is_logging_enabled)
    {
        event_system->is_logging_enabled = is_logging_enabled;

        for (i32 i = 0; i < EventType_Count; i++)
        {
            Event_Registry &registry = event_system->registry[i];
            registry.entry_count = 0;
            registry.entries     = ArenaPushArrayAlignedZero(arena,
                                                             Event_Entry,
                                                             MAX_EVENT_ENTRY_COUNT_PER_TYPE);
        }
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
        Event_Registry &registry = event_system->registry[event_type];

        for (u32 i = 0; i < registry.entry_count; i++)
        {
            Event_Entry &entry = registry.entries[i];
            if (entry.sender == sender && entry.on_event == on_event)
            {
                fprintf(stderr,
                        "[WARNING]: %s event is already registered with sender %p\n",
                        convert_event_type_to_string(event_type),
                        sender);
                return false;
            }
        }

        Assert(registry.entry_count < MAX_EVENT_ENTRY_COUNT_PER_TYPE);
        Event_Entry entry;
        entry.on_event = on_event;
        entry.sender   = sender;
        registry.entries[registry.entry_count++] = entry;
        fprintf(stderr,
                "[TRACE]: %s event registered with sender %p\n",
                convert_event_type_to_string(event_type),
                sender);
        return true;
    }

    bool unregister_event(Event_System *event_system, EventType event_type, void *sender)
    {
        Event_Registry &registry = event_system->registry[event_type];

        for (u32 i = 0; i < registry.entry_count; i++)
        {
            if (registry.entries[i].sender == sender)
            {
                for (u32 j = i + 1; j < registry.entry_count; ++j)
                {
                    Event_Entry temp = registry.entries[j];
                    registry.entries[j] = registry.entries[j - 1];
                    registry.entries[j - 1] = temp;
                }

                registry.entry_count--;

                fprintf(stderr,
                        "[TRACE]: %s event unregistered with sender %p\n",
                        convert_event_type_to_string(event_type),
                        sender);

                return true;
            }
        }

        fprintf(stderr,
                "[WARNING]: %s event is not registered with sender %p\n",
                convert_event_type_to_string(event_type),
                sender);

        return false;
    }

    void fire_event(Event_System *event_system, EventType event_type, const Event *event)
    {
        Event_Registry &registry = event_system->registry[event_type];

        for (i32 i = (i32)registry.entry_count - 1; i >= 0; i--)
        {
            Event_Entry &entry = registry.entries[i];
            bool handled       = entry.on_event(event, entry.sender);

            if (event_system->is_logging_enabled)
            {
                const char* event_string = convert_event_to_string(event_type, event);
                fprintf(stderr,
                        "[TRACE]: %s event fired with sender %p\n",
                        event_string,
                        entry.sender);
            }

            if (handled)
            {
                break;
            }
        }
    }

    Event make_resize_event(u32 width, u32 height)
    {
        Event event;
        event.data_u32_array[0] = width;
        event.data_u32_array[1] = height;
        return event;
    }

    void parse_resize_event(const Event *event, u32 *out_width, u32 *out_height)
    {
        *out_width  = event->data_u32_array[0];
        *out_height = event->data_u32_array[1];
    }

    Event make_key_pressed_event(u16 key)
    {
        Event event;
        event.data_u16 = key;
        return event;
    }

    Event make_key_released_event(u16 key)
    {
        Event event;
        event.data_u16 = key;
        return event;
    }

    Event make_key_held_event(u16 key)
    {
        Event event;
        event.data_u16 = key;
        return event;
    }

    void parse_key_code(const Event *event, u16 *out_key)
    {
        *out_key = event->data_u16;
    }

    Event make_mouse_move_event(f32 mouse_x, f32 mouse_y)
    {
        Event event;
        event.data_f32_array[0] = mouse_x;
        event.data_f32_array[1] = mouse_y;
        return event;
    }

    void parse_mouse_move(const Event *event, f32 *out_mouse_x, f32 *out_mouse_y)
    {
        *out_mouse_x = event->data_f32_array[0];
        *out_mouse_y = event->data_f32_array[1];
    }

    Event make_button_pressed_event(u8 button)
    {
        Event event;
        event.data_u8 = button;
        return event;
    }

    Event make_button_released_event(u8 button)
    {
        Event event;
        event.data_u8 = button;
        return event;
    }

    Event make_button_held_event(u8 button)
    {
        Event event;
        event.data_u8 = button;
        return event;
    }

    void parse_button_code(const Event *event, u8 *out_button)
    {
        *out_button = event->data_u8;
    }

    Event make_mouse_wheel_event(f32 xoffset, f32 yoffset)
    {
        Event event;
        event.data_f32_array[0] = xoffset;
        event.data_f32_array[1] = yoffset;
        return event;
    }

    void parse_mouse_wheel(const Event *event, f32 *out_xoffset, f32 *out_yoffset)
    {
        *out_xoffset = event->data_f32_array[0];
        *out_yoffset = event->data_f32_array[1];
    }

    Event make_char_event(char code_point)
    {
        Event event;
        event.data_u8 = code_point;
        return event;
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

            default:
            {
                return "";
            } break;
        }

        return "";
    }
}