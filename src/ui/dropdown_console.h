#pragma once

#include "core/common.h"
#include "memory/memory_arena.h"
#include "containers/string.h"

#include <mutex>
#include <glm/glm.hpp>

namespace minecraft {

    struct Bitmap_Font;
    struct Event_System;
    struct Memory_Arena;

    enum ConsoleState
    {
        ConsoleState_Closed,
        ConsoleState_HalfOpen,
        ConsoleState_FullOpen
    };

    struct Dropdown_Console_Line_Info
    {
        String8 str;
        bool    is_command;
        bool    is_command_succeeded;
    };

    struct Dropdown_Console
    {
        Memory_Arena string_input_arena;
        Memory_Arena string_arena;
        Memory_Arena line_arena;

        std::mutex *push_line_mutex;

        ConsoleState state;

        i32 current_cursor_index;

        u32                         line_count;
        Dropdown_Console_Line_Info *lines;

        Bitmap_Font *font;

        glm::vec4 text_color;
        glm::vec4 background_color;

        glm::vec4 input_text_background_color;
        glm::vec4 input_text_color;

        glm::vec4 input_text_cursor_color;
        glm::vec2 input_text_cursor_size;

        glm::vec4 scroll_bar_background_color;
        glm::vec4 scroll_bar_color;

        glm::vec4 command_succeeded_color;
        glm::vec4 command_failed_color;

        String8 current_text;

        f32 cursor_cooldown_time;
        f32 cursor_current_cooldown_time;
        f32 cursor_opacity_limit;
        f32 cursor_opacity;

        f32 toggle_speed;

        f32 padding_x;
        f32 y_extent;
        f32 y_extent_target;

        f32 scroll_bar_width;
        f32 scroll_speed;

        f32 scroll_y;
        f32 scroll_y_target;

        f32 scroll_x;
        f32 scroll_x_target;
    };

    bool initialize_dropdown_console(Dropdown_Console *console,
                                     Memory_Arena     *arena,
                                     Bitmap_Font      *font,
                                     Event_System     *event_system,
                                     const glm::vec4  &text_color,
                                     const glm::vec4  &background_color,
                                     const glm::vec4  &input_text_color,
                                     const glm::vec4  &input_text_background_color,
                                     const glm::vec4  &input_text_cursor_color,
                                     const glm::vec4  &scroll_bar_background_color,
                                     const glm::vec4  &scroll_bar_color,
                                     const glm::vec4  &command_succeeded_color,
                                     const glm::vec4  &command_failed_color);

    void shutdown_dropdown_console(Dropdown_Console *console);
    void toggle_dropdown_console(Dropdown_Console *console);
    void clear_dropdown_console(Dropdown_Console *console);

    void open_dropdown_console_with_half_extent(Dropdown_Console *console);
    void open_dropdown_console_with_full_extent(Dropdown_Console *console);

    void close_dropdown_console(Dropdown_Console *console);

    void draw_dropdown_console(Dropdown_Console *console, f32 delta_time);

    Dropdown_Console_Line_Info& push_line(Dropdown_Console *console,
                                          String8 line = Str8(""),
                                          bool is_command = false,
                                          bool is_command_succeeded = false);

    Dropdown_Console_Line_Info& thread_safe_push_line(Dropdown_Console *console,
                                                      String8 line = Str8(""),
                                                      bool is_command = false,
                                                      bool is_command_succeeded = false);
}