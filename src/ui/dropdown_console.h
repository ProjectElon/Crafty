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

    struct Dropdown_Console_Data
    {
        glm::vec4 text_color;
        glm::vec4 background_color;

        glm::vec4 input_text_background_color;
        glm::vec4 input_text_color;

        glm::vec4 input_text_cursor_color;

        glm::vec2 input_text_cursor_size;

        ConsoleState state;
        Bitmap_Font *font;

        f32 left_padding;

        std::string current_text;
        i32 current_cursor_index;

        f32 cursor_cooldown_time;
        f32 cursor_current_cooldown_time;
        f32 cursor_opacity;

        f32 y_extent;
        f32 y_extent_target;
        f32 toggle_speed;

        std::vector<std::string> history;
    };

    struct Dropdown_Console
    {
        static Dropdown_Console_Data internal_data;

        static bool initialize(Bitmap_Font *font,
                               const glm::vec4& text_color,
                               const glm::vec4& background_color,
                               const glm::vec4& input_text_color,
                               const glm::vec4& input_text_background_color,
                               const glm::vec4& input_text_cursor_color);
        static void shutdown();

        static void toggle();
        static bool on_char_input(const Event *event, void *sender);
        static bool on_key(const Event *event, void *sender);

        static void draw(f32 dt);
        static void clear(const std::vector<Command_Argument>& args);
        static void log(const std::string& text);
    };
}