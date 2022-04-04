#include "dropdown_console.h"
#include "renderer/opengl_2d_renderer.h"
#include "renderer/opengl_renderer.h"
#include "renderer/font.h"
#include "core/input.h"

namespace minecraft {

    bool Dropdown_Console::initialize(Bitmap_Font *font,
                                      const glm::vec4& text_color,
                                      const glm::vec4& background_color,
                                      const glm::vec4& input_text_color,
                                      const glm::vec4& input_text_background_color,
                                      const glm::vec4& input_text_cursor_color)
    {
        internal_data.state = ConsoleState_Closed,
        internal_data.font = font;

        internal_data.text_color = text_color;
        internal_data.background_color = background_color;

        internal_data.input_text_color = input_text_color;
        internal_data.input_text_background_color = input_text_background_color;

        internal_data.input_text_cursor_color = input_text_cursor_color;

        internal_data.input_text_cursor_size = { 10.0f, font->char_height };

        internal_data.input_text_cursor_opacity = 1.0f;
        internal_data.current_text = "";
        internal_data.current_cursor_index = 0;

        return true;
    }

    void Dropdown_Console::shutdown()
    {
    }

    void Dropdown_Console::toggle()
    {
        ConsoleState& state = internal_data.state;

        if (state == ConsoleState_Closed)
        {
            state = ConsoleState_Open;
        }
        else if (state == ConsoleState_Open)
        {
            state = ConsoleState_Closed;
        }
    }

    bool Dropdown_Console::on_char_input(const Event *event, void *sender)
    {
        auto& current_text = internal_data.current_text;
        auto& current_cursor_index = internal_data.current_cursor_index;

        if (internal_data.state == ConsoleState_Open)
        {
            char code_point;
            Event_System::parse_char(event, &code_point);
            current_text.insert(current_text.begin() + current_cursor_index, code_point);
            current_cursor_index++;
            return true;
        }

        return false;
    }

    bool Dropdown_Console::on_key(const Event *event, void *sender)
    {
        auto& current_text = internal_data.current_text;
        auto& current_cursor_index = internal_data.current_cursor_index;
        auto& history = internal_data.history;

        if (internal_data.state == ConsoleState_Open)
        {
            u16 key;
            Event_System::parse_key_code(event, &key);

            if (key == MC_KEY_ENTER)
            {
                if (current_text.size())
                {
                    history.push_back(current_text);
                    current_text = "";
                    current_cursor_index = 0;
                }
            }
            else if (key == MC_KEY_BACKSPACE)
            {
                if (current_cursor_index >= 1)
                {
                    current_text.erase(current_text.begin() + current_cursor_index - 1);
                    current_cursor_index--;
                }
            }
            else if (key == MC_KEY_LEFT)
            {
                if (current_cursor_index >= 1) current_cursor_index--;
            }
            else if (key == MC_KEY_RIGHT)
            {
                if (current_cursor_index < current_text.size()) current_cursor_index++;
            }

            return true;
        }

        return false;
    }

    void Dropdown_Console::draw(f32 dt)
    {
        ConsoleState& state = internal_data.state;
        Bitmap_Font *font = internal_data.font;
        glm::vec2 frame_buffer_size = Opengl_Renderer::get_frame_buffer_size();
        auto& history = internal_data.history;

        if (state == ConsoleState_Open)
        {
            glm::vec2 console_position = frame_buffer_size * 0.5f;
            glm::vec2 console_scale = { frame_buffer_size.x, frame_buffer_size.y };
            Opengl_2D_Renderer::draw_rect(console_position, console_scale, 0.0f, internal_data.background_color);

            f32 left_padding = 10.0f;
            glm::vec2 cursor = { left_padding, frame_buffer_size.y - 4.0f * font->char_height };
            i32 count = history.size();
            for (i32 i = count - 1; i >= 0; i--)
            {
                const std::string& text = history[i];
                glm::vec2 text_size = font->get_string_size(text);
                Opengl_2D_Renderer::draw_string(font, text, text_size, cursor + text_size * 0.5f, internal_data.text_color);
                cursor.y -= font->char_height;
            }

            glm::vec2 input_text_position = { frame_buffer_size.x * 0.5f, frame_buffer_size.y - font->char_height };
            glm::vec2 input_text_scale = { frame_buffer_size.x, 2.0f * font->char_height };
            Opengl_2D_Renderer::draw_rect(input_text_position, input_text_scale, 0.0f, internal_data.input_text_background_color);

            glm::vec2 current_text_size = font->get_string_size(internal_data.current_text);
            Opengl_2D_Renderer::draw_string(font, internal_data.current_text, current_text_size, glm::vec2(left_padding, frame_buffer_size.y - font->char_height) + glm::vec2(current_text_size.x * 0.5f, 0.0f), internal_data.input_text_color);

            std::string sub_string = internal_data.current_text.substr(0, internal_data.current_cursor_index);
            glm::vec2 sub_string_size = font->get_string_size(sub_string);
            glm::vec2 input_text_cursor_position = 
            { left_padding + internal_data.input_text_cursor_size.x * 0.5f + sub_string_size.x, frame_buffer_size.y - font->char_height };
            
            f32& cursor_opacity = internal_data.input_text_cursor_opacity;
            cursor_opacity += dt * 90.0f;
            if (cursor_opacity >= 360.0f) cursor_opacity -= 360.0f;

            glm::vec4 input_text_cursor_color = internal_data.input_text_cursor_color;

            input_text_cursor_color.a = glm::abs(glm::sin(glm::radians(cursor_opacity)));

            Opengl_2D_Renderer::draw_rect(input_text_cursor_position, internal_data.input_text_cursor_size, 0.0f, input_text_cursor_color);
        }
    }

    Dropdown_Console_Data Dropdown_Console::internal_data;
}