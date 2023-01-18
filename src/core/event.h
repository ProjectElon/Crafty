#pragma once

#include "common.h"

#define MAX_EVENT_ENTRY_COUNT_PER_TYPE 1024

namespace minecraft {

    struct Memory_Arena;

    enum EventType
    {
        // Window Events
        EventType_Resize,
        EventType_Minimize,
        EventType_Restore,
        EventType_Quit,

        // Key Events
        EventType_KeyPress,
        EventType_KeyHeld,
        EventType_KeyRelease,
        EventType_Char,

        // Mouse Events
        EventType_MouseButtonPress,
        EventType_MouseButtonHeld,
        EventType_MouseButtonRelease,
        EventType_MouseWheel,
        EventType_MouseMove,

        EventType_Count
    };

    // 128 bit
    union Event
    {
        u8  data_u8;
        i8  data_i8;
        u16 data_u16;
        i16 data_i16;
        u32 data_u32;
        i32 data_i32;
        u64 data_u64;
        i64 data_i64;
        f32 data_f32;
        f64 data_f64;

        u8  data_u8_array[16];
        i8  data_i8_array[16];
        u16 data_u16_array[8];
        i16 data_i16_array[8];
        u32 data_u32_array[4];
        i32 data_i32_array[4];
        u64 data_u64_array[2];
        i64 data_i64_array[2];
        f32 data_f32_array[4];
        f64 data_f64_array[2];

        char data_string[16];
    };

    typedef bool (*on_event_fn)(const Event *event, void *sender);

    struct Event_Entry
    {
        void        *sender;
        on_event_fn  on_event;
    };

    struct Event_Registry
    {
        u32          entry_count;
        Event_Entry *entries;
    };

    struct Event_System
    {
        Memory_Arena   *arena;
        Event_Registry registry[EventType_Count];
        bool           is_logging_enabled;
    };

    bool initialize_event_system(Event_System *event_system,
                                 Memory_Arena *arena,
                                 bool is_logging_enabled);

    void shutdown_event_system(Event_System *event_system);

    bool register_event(Event_System *event_system,
                        EventType event_type,
                        on_event_fn on_event,
                        void *sender = nullptr);

    bool unregister_event(Event_System *event_system,
                          EventType event_type,
                          void *sender = nullptr);

    void fire_event(Event_System *event_system,
                    EventType event_type,
                    const Event *event);

    Event make_resize_event(u32 width, u32 height);
    Event make_key_pressed_event(u16 key);
    Event make_key_released_event(u16 key);
    Event make_key_held_event(u16 key);
    Event make_mouse_move_event(f32 mouse_x, f32 mouse_y);
    Event make_button_pressed_event(u8 button);
    Event make_button_released_event(u8 button);
    Event make_button_held_event(u8 button);
    Event make_mouse_wheel_event(f32 xoffset, f32 yoffset);
    Event make_char_event(char code_point);

    void parse_resize_event(const Event *event,
                            u32 *out_width,
                            u32 *out_height);

    void parse_key_code(const Event *event,
                        u16 *out_key);

    void parse_mouse_move(const Event *event,
                          f32 *out_mouse_x,
                          f32 *out_mouse_y);

    void parse_button_code(const Event *event,
                           u8 *out_button);

    void parse_mouse_wheel(const Event *event,
                           f32 *out_xoffset,
                           f32 *out_yoffset);

    void parse_char(const Event *event,
                    char *out_code_point);

    const char* convert_event_type_to_string(EventType event_type);
    const char* convert_event_to_string(EventType event_type, const Event *event);
}