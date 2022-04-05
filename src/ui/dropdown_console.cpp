#include "dropdown_console.h"
#include "renderer/opengl_2d_renderer.h"
#include "renderer/opengl_renderer.h"
#include "renderer/font.h"
#include "core/input.h"

#include <glm/gtx/compatibility.hpp>

namespace minecraft {

    bool Dropdown_Console::initialize(Bitmap_Font *font,
                                      const glm::vec4& text_color,
                                      const glm::vec4& background_color,
                                      const glm::vec4& input_text_color,
                                      const glm::vec4& input_text_background_color,
                                      const glm::vec4& input_text_cursor_color)
    {
        internal_data.state = ConsoleState_Closed;
        internal_data.font = font;
        internal_data.left_padding = 10.0f;

        internal_data.text_color = text_color;
        internal_data.background_color = background_color;

        internal_data.input_text_color = input_text_color;
        internal_data.input_text_background_color = input_text_background_color;

        internal_data.input_text_cursor_color = input_text_cursor_color;
        internal_data.cursor_current_cooldown_time = 0.0f;
        internal_data.cursor_cooldown_time = 0.5f;
        internal_data.input_text_cursor_size = { font->size_in_pixels / 2.0f, font->char_height * 1.4f };
        internal_data.cursor_opacity = 0.0f;

        internal_data.current_text = "";
        internal_data.current_cursor_index = 0;

        internal_data.y_extent = 0.0f;
        internal_data.y_extent_target = 0.0f;
        internal_data.toggle_speed = 5.0f;

        return true;
    }

    void Dropdown_Console::shutdown()
    {
    }

    void Dropdown_Console::toggle()
    {
        ConsoleState& state = internal_data.state;
        f32& y_extent_target = internal_data.y_extent_target;

        if (state == ConsoleState_Closed)
        {
            state = ConsoleState_Half_Open;
            y_extent_target = 0.5f;
        }
        else if (state == ConsoleState_Half_Open)
        {
            state = ConsoleState_Full_Open;
            y_extent_target = 1.0f;
        }
        else if (state == ConsoleState_Full_Open)
        {
            state = ConsoleState_Closed;
            y_extent_target = 0.0f;
        }
    }

    bool Dropdown_Console::on_char_input(const Event *event, void *sender)
    {
        auto& current_text = internal_data.current_text;
        auto& current_cursor_index = internal_data.current_cursor_index;

        if (internal_data.state != ConsoleState_Closed)
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

        if (internal_data.state != ConsoleState_Closed)
        {
            internal_data.cursor_current_cooldown_time = internal_data.cursor_cooldown_time;
            internal_data.cursor_opacity = 180.0f;

            u16 key;
            Event_System::parse_key_code(event, &key);

            if (key == MC_KEY_ENTER || key == MC_KEY_KP_ENTER)
            {
                std::vector<std::string> tokens = console::parse_command(current_text);

                if (tokens.size())
                {
                    std::string name = tokens[0];
                    std::vector<Command_Argument> args;
                    
                    for (u32 i = 1; i < (i32)tokens.size(); i++) 
                    {
                        args.push_back({ CommandArgumentType_String, tokens[i] });
                    }

                    const Console_Command *command = console::find_command(name, args);

                    if (command)
                    {
                        history.push_back(current_text);
                        command->execute(args);
                    }
                    else
                    {
                        history.push_back("\"" + current_text + "\" is not a registered command");
                    }
                }
                else
                {
                    history.push_back("");
                }

                current_text = "";
                current_cursor_index = 0;
            }
            else if (key == MC_KEY_BACKSPACE || key == MC_KEY_DELETE)
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

    void Dropdown_Console::log(const std::string& text)
    {
        internal_data.history.push_back(text);
    }

    void Dropdown_Console::draw(f32 dt)
    {
        ConsoleState& state = internal_data.state;
        f32& y_extent = internal_data.y_extent;
        f32 y_extent_target = internal_data.y_extent_target;
        f32& cursor_current_cooldown_time = internal_data.cursor_current_cooldown_time;
        Bitmap_Font *font = internal_data.font;
        auto& history = internal_data.history;

        y_extent = glm::lerp(y_extent, y_extent_target, dt * internal_data.toggle_speed);

        if (cursor_current_cooldown_time >= 0.0f) cursor_current_cooldown_time -= dt;

        glm::vec2 frame_buffer_size = Opengl_Renderer::get_frame_buffer_size();

        glm::vec2 console_position = { frame_buffer_size.x * 0.5f, frame_buffer_size.y * y_extent * 0.5f };
        glm::vec2 console_scale = { frame_buffer_size.x, frame_buffer_size.y * y_extent };
        Opengl_2D_Renderer::draw_rect(console_position, console_scale, 0.0f, internal_data.background_color);

        glm::vec2 cursor = { internal_data.left_padding, frame_buffer_size.y * y_extent - 4.0f * font->char_height };
        i32 count = history.size();
        for (i32 i = count - 1; i >= 0; i--)
        {
            const std::string& text = history[i];
            glm::vec2 text_size = font->get_string_size(text);
            Opengl_2D_Renderer::draw_string(font, text, text_size, cursor + text_size * 0.5f, internal_data.text_color);
            cursor.y -= font->char_height;
        }

        glm::vec2 input_text_position = { frame_buffer_size.x * 0.5f, frame_buffer_size.y * y_extent - font->char_height };
        glm::vec2 input_text_scale = { frame_buffer_size.x, 2.0f * font->char_height };
        Opengl_2D_Renderer::draw_rect(input_text_position, input_text_scale, 0.0f, internal_data.input_text_background_color);

        glm::vec2 current_text_size = font->get_string_size(internal_data.current_text);
        Opengl_2D_Renderer::draw_string(font,
                                        internal_data.current_text,
                                        current_text_size,
                                        glm::vec2(internal_data.left_padding, frame_buffer_size.y * y_extent - font->char_height) + glm::vec2(current_text_size.x * 0.5f, 0.0f),
                                        internal_data.input_text_color);

        std::string sub_string = internal_data.current_text.substr(0, internal_data.current_cursor_index);
        glm::vec2 sub_string_size = font->get_string_size(sub_string);
        glm::vec2 input_text_cursor_position = { internal_data.left_padding + internal_data.input_text_cursor_size.x * 0.5f + sub_string_size.x, frame_buffer_size.y * y_extent - font->char_height };

        glm::vec4 input_text_cursor_color = internal_data.input_text_cursor_color;
        f32 &cursor_opacity = internal_data.cursor_opacity;

        if (cursor_current_cooldown_time <= 0.0f)
        {
            cursor_opacity += dt * 180.0f;
            if (cursor_opacity >= 360.0f) cursor_opacity -= 360.0f;
            input_text_cursor_color.a = glm::abs(glm::sin(glm::radians(cursor_opacity)));
        }

        Opengl_2D_Renderer::draw_rect(input_text_cursor_position, internal_data.input_text_cursor_size, 0.0f, input_text_cursor_color);
    }

    void Dropdown_Console::clear(const std::vector<Command_Argument>& args)
    {
        internal_data.history.resize(0);
    }

    Dropdown_Console_Data Dropdown_Console::internal_data;
}