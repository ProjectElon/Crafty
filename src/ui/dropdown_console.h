#pragma once

#include "core/common.h"
#include "core/event.h"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace minecraft {

    struct Bitmap_Font;

    enum ConsoleState
    {
        ConsoleState_Closed,
        ConsoleState_Open
    };

    struct Dropdown_Console_Data
    {
        ConsoleState state;
        Bitmap_Font *font;

        glm::vec4 text_color;
        glm::vec4 background_color;

        glm::vec4 input_text_background_color;
        glm::vec4 input_text_color;

        glm::vec4 input_text_cursor_color;

        glm::vec2 input_text_cursor_size;
        f32 input_text_cursor_opacity;

        std::string current_text;
        i32 current_cursor_index;

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
    };
}