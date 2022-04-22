#pragma once

#include "core/common.h"
#include "core/event.h"

#include "game/console_commands.h"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace minecraft {

    struct Bitmap_Font;

    enum ConsoleState
    {
        ConsoleState_Closed,
        ConsoleState_Half_Open,
        ConsoleState_Full_Open
    };

    struct Dropdown_Console_Word
    {
        std::string text;
        glm::vec4   color;
    };

    struct Dropdown_Console_Line
    {
        std::vector<Dropdown_Console_Word> words;
    };

    struct Dropdown_Console_Data
    {
        glm::vec4 text_color;
        glm::vec4 background_color;

        glm::vec4 input_text_background_color;
        glm::vec4 input_text_color;

        glm::vec4 input_text_cursor_color;
        glm::vec2 input_text_cursor_size;

        glm::vec4 scroll_bar_background_color;
        glm::vec4 scroll_bar_color;

        glm::vec4 command_color;
        glm::vec4 argument_color;
        glm::vec4 type_color;

        ConsoleState state;
        Bitmap_Font *font;

        f32 padding_x;

        std::string current_text;
        i32 current_cursor_index;

        f32 cursor_cooldown_time;
        f32 cursor_current_cooldown_time;
        f32 cursor_opacity_limit;
        f32 cursor_opacity;

        f32 toggle_speed;

        f32 y_extent;
        f32 y_extent_target;

        f32 scroll_bar_width;
        f32 scroll_speed;

        f32 scroll_y;
        f32 scroll_y_target;

        f32 scroll_x;
        f32 scroll_x_target;

        std::vector< Dropdown_Console_Line > history;
    };

    struct Dropdown_Console
    {
        static Dropdown_Console_Data internal_data;

        static bool initialize(Bitmap_Font *font,
                               const glm::vec4& text_color,
                               const glm::vec4& background_color,
                               const glm::vec4& input_text_color,
                               const glm::vec4& input_text_background_color,
                               const glm::vec4& input_text_cursor_color,
                               const glm::vec4& scroll_bar_background_color,
                               const glm::vec4& scroll_bar_color,
                               const glm::vec4& command_color,
                               const glm::vec4& argument_color,
                               const glm::vec4& type_color);
        static void shutdown();

        static void toggle();

        static bool on_char_input(const Event *event, void *sender);
        static bool on_key(const Event *event, void *sender);
        static bool on_mouse_wheel(const Event *event, void *sender);

        static void clear();

        static void open_with_half_extent();
        static void open_with_full_extent();

        static void close();

        inline static bool is_closed() { return internal_data.state == ConsoleState_Closed; }

        static f32 get_text_height();
        static f32 get_line_height();
        static f32 get_size_y();
        static f32 get_max_scroll_y();

        static void draw(f32 dt);

        static void log(const std::string& text, const glm::vec4& color = internal_data.text_color);
        static void log_with_new_line(const std::string& text, const glm::vec4& color = internal_data.text_color);
    };
}