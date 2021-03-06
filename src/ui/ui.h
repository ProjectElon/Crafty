#pragma once

#include "core/common.h"
#include <glm/glm.hpp>
#include <string>

namespace minecraft {

    struct Bitmap_Font;
    struct Opengl_Texture;

    struct UI_State
    {
        glm::vec2 cursor;
        glm::vec2 offset;
        glm::vec4 text_color;
        glm::vec4 fill_color;
        Bitmap_Font *font;
    };

    struct UI_Data
    {
        UI_State default_state;
        UI_State current_state;
    };

    struct UI
    {
        static UI_Data internal_data;

        static bool initialize(UI_State *default_state);
        static void shutdown();

        static void begin();

        static void set_font(Bitmap_Font *font);
        static void set_text_color(const glm::vec4& color);
        static void set_fill_color(const glm::vec4& color);

        static void set_cursor(const glm::vec2& cursor);
        static void set_offset(const glm::vec2& offset);

        static bool rect(const glm::vec2& size);
        static bool text(const std::string& text);
        static bool button(const std::string& text, const glm::vec2& padding = { 5.0f, 5.0f });

        static bool textured_button(const std::string& text,
                                    Opengl_Texture *texture,
                                    const glm::vec2& padding   = { 5.0f, 5.0f },
                                    const glm::vec2& uv_scale  = { 1.0f, 1.0f },
                                    const glm::vec2& uv_offset = { 0.0f, 0.0f });

        static void end();
    };
}
