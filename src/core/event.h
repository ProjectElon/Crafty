#pragma once

#include "common.h"
#include <vector>

namespace minecraft {

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
        void *sender;
        on_event_fn on_event;
    };

    struct Event_Registry
    {
        std::vector<Event_Entry> entries; // todo(harlequin): data structures
    };

    struct Event_System_Data
    {
        bool is_tracing_events = false;
        Event_Registry event_registry[EventType_Count];
    };

    struct Event_System
    {
        static Event_System_Data internal_data;

        static bool initialize(struct Platform *platform, bool is_tracing_events = false);
        static void shutdown();

        static bool register_event(EventType event_type, on_event_fn on_event, void *sender);
        static bool unregister_event(EventType event_type, void *sender);

        static void fire_event(EventType event_type, const Event *event);

        static void parse_resize_event(const Event *event, u32 *out_width, u32 *out_height);
        static void parse_key_code(const Event *event, u16 *out_key);
        static void parse_mouse_move(const Event *event, f32 *out_mouse_x, f32 *out_mouse_y);
        static void parse_button_code(const Event *event, u8 *out_button);
        static void parse_model_wheel(const Event *event, f32 *out_xoffset, f32 *out_yoffset);
        static void parse_char(const Event *event, char *out_code_point);
    };
}