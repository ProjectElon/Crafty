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
                                      const glm::vec4& input_text_cursor_color,
                                      const glm::vec4& scroll_bar_background_color,
                                      const glm::vec4& scroll_bar_color,
                                      const glm::vec4& command_color,
                                      const glm::vec4& argument_color,
                                      const glm::vec4& type_color)
    {
        internal_data.state = ConsoleState_Closed;
        internal_data.font = font;
        internal_data.left_padding = (font->size_in_pixels / 2.0f);

        internal_data.text_color = text_color;
        internal_data.background_color = background_color;

        internal_data.input_text_color = input_text_color;
        internal_data.input_text_background_color = input_text_background_color;

        internal_data.scroll_bar_background_color = scroll_bar_background_color;
        internal_data.scroll_bar_color = scroll_bar_color;

        internal_data.command_color  = command_color;
        internal_data.argument_color = argument_color;
        internal_data.type_color     = type_color;

        internal_data.input_text_cursor_color = input_text_cursor_color;
        internal_data.cursor_current_cooldown_time = 0.0f;
        internal_data.cursor_cooldown_time = 1.0f;
        internal_data.input_text_cursor_size = { font->size_in_pixels / 2.0f, font->char_height * 1.4f };
        internal_data.cursor_opacity_limit = 0.7f;
        internal_data.cursor_opacity = 0.0f;

        internal_data.current_text = "";
        internal_data.current_cursor_index = 0;

        internal_data.y_extent = 0.0f;
        internal_data.y_extent_target = 0.0f;
        internal_data.toggle_speed = 10.0f;

        internal_data.scroll_speed = font->char_height * 10.0f;

        internal_data.scroll_y = 0.0f;
        internal_data.scroll_y_target = 0.0f;

        internal_data.scroll_x = 0.0f;
        internal_data.scroll_x_target = 0.0f;

        // todo(harlequin): temprary to be removed
        {
            log_with_new_line("first line", command_color);

            for (i32 i = 0; i < 100; i++)
            {
                std::string no = std::to_string(i);
                log_with_new_line(no + ". the name is quin harlequin.", command_color);
            }

            log_with_new_line("last line", command_color);
        }

        Event_System::register_event(EventType_Char,       Dropdown_Console::on_char_input,  &internal_data);
        Event_System::register_event(EventType_KeyPress,   Dropdown_Console::on_key,         &internal_data);
        Event_System::register_event(EventType_KeyHeld,    Dropdown_Console::on_key,         &internal_data);
        Event_System::register_event(EventType_MouseWheel, Dropdown_Console::on_mouse_wheel, &internal_data);

        console_commands::register_commands();

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
        auto& scroll_y_target = internal_data.scroll_y_target;

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
        auto& scroll_y_target = internal_data.scroll_y_target;

        if (internal_data.state != ConsoleState_Closed)
        {
            internal_data.cursor_current_cooldown_time = internal_data.cursor_cooldown_time;
            internal_data.cursor_opacity = 180.0f;

            u16 key;
            Event_System::parse_key_code(event, &key);

            if (key == MC_KEY_ENTER || key == MC_KEY_KP_ENTER)
            {
                scroll_y_target = 0.0f;

                if (current_text.empty())
                {
                    log_with_new_line("");
                }
                else
                {
                    Console_Command_Parse_Result parse_result = console_commands::parse_command(current_text);
                    const Console_Command *command = console_commands::find_command_from_parse_result(parse_result);

                    if (command)
                    {
                        log("-> ");
                        log_with_new_line(current_text, internal_data.command_color);

                        command->execute(parse_result.args);
                    }
                    else
                    {
                        log("\"" + current_text + "\"", internal_data.command_color);
                        log_with_new_line(" is not a registered command");
                    }
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

    bool Dropdown_Console::on_mouse_wheel(const Event *event, void *sender)
    {
        f32 x_offset;
        f32 y_offset;
        Event_System::parse_model_wheel(event, &x_offset, &y_offset);

        glm::vec2 frame_buffer_size = Opengl_Renderer::get_frame_buffer_size();

        f32 text_height = internal_data.history.size() * internal_data.font->char_height * 1.3f;

        f32 max_scroll_y = 0.0f;
        f32 console_y_size = frame_buffer_size.y * internal_data.y_extent - 2.0f * internal_data.font->char_height;

        if (text_height > console_y_size)
        {
            max_scroll_y = text_height - console_y_size + internal_data.font->char_height * 1.3f;
        }

        internal_data.scroll_y_target += y_offset * internal_data.scroll_speed;
        internal_data.scroll_y_target = glm::clamp(internal_data.scroll_y_target, 0.0f, max_scroll_y);

        return true;
    }

    void Dropdown_Console::log(const std::string& text, const glm::vec4& color)
    {
        if (internal_data.history.size() == 0 || text == "\n")
        {
            Dropdown_Console_Line line = {};
            internal_data.history.push_back(line);
        }

        if (text != "\n")
        {
            auto& line = internal_data.history.back();
            line.words.push_back({ text, color });
        }
    }

    void Dropdown_Console::log_with_new_line(const std::string& text, const glm::vec4& color)
    {
        log(text, color);
        log("\n");
    }

    void Dropdown_Console::clear()
    {
        internal_data.history.resize(0);
    }

    void Dropdown_Console::open_with_half_extent()
    {
        auto& state = Dropdown_Console::internal_data.state;
        auto& y_extent_target = Dropdown_Console::internal_data.y_extent_target;

        if (state == ConsoleState_Closed)
        {
            state = ConsoleState_Half_Open;
            y_extent_target = 0.5f;
        }
    }

    void Dropdown_Console::open_with_full_extent()
    {
        auto& state = Dropdown_Console::internal_data.state;
        auto& y_extent_target = Dropdown_Console::internal_data.y_extent_target;

        if (state == ConsoleState_Closed)
        {
            state = ConsoleState_Full_Open;
            y_extent_target = 1.0f;
        }
    }

    void Dropdown_Console::close()
    {
        auto& state = Dropdown_Console::internal_data.state;
        auto& y_extent_target = Dropdown_Console::internal_data.y_extent_target;

        if (state == ConsoleState_Half_Open || state == ConsoleState_Full_Open)
        {
            state = ConsoleState_Closed;
            y_extent_target = 0.0f;
        }
    }

    void Dropdown_Console::draw(f32 dt)
    {
        ConsoleState& state = internal_data.state;
        f32& y_extent = internal_data.y_extent;
        f32 y_extent_target = internal_data.y_extent_target;
        f32& cursor_current_cooldown_time = internal_data.cursor_current_cooldown_time;
        Bitmap_Font *font = internal_data.font;
        auto& history = internal_data.history;
        auto& scroll_y = internal_data.scroll_y;
        auto& scroll_y_target = internal_data.scroll_y_target;
        auto& scroll_x = internal_data.scroll_x;
        auto& scroll_x_target = internal_data.scroll_x_target;
        auto& left_padding = internal_data.left_padding;

        y_extent = glm::lerp(y_extent, y_extent_target, dt * internal_data.toggle_speed);
        y_extent = glm::clamp(y_extent, 0.0f, 1.0f);

        if (cursor_current_cooldown_time >= 0.0f) cursor_current_cooldown_time -= dt;

        glm::vec2 frame_buffer_size = Opengl_Renderer::get_frame_buffer_size();

        glm::vec2 console_position = { frame_buffer_size.x * 0.5f, frame_buffer_size.y * y_extent * 0.5f };
        glm::vec2 console_scale = { frame_buffer_size.x, frame_buffer_size.y * y_extent };
        Opengl_2D_Renderer::draw_rect(console_position, console_scale, 0.0f, internal_data.background_color);

        glm::vec2 cursor = { internal_data.left_padding, frame_buffer_size.y * y_extent - 4.0f * font->char_height + scroll_y };

        i32 count = (i32)history.size();
        f32 text_height = count * font->char_height * 1.3f;
        for (i32 i = count - 1; i >= 0; i--)
        {
            const Dropdown_Console_Line& line = history[i];

            for (i32 j = 0; j < (i32)line.words.size(); ++j)
            {
                const Dropdown_Console_Word& word = line.words[j];

                glm::vec2 text_size = font->get_string_size(word.text);

                if (cursor.y <= frame_buffer_size.y * y_extent - 4.0f * font->char_height)
                {
                    Opengl_2D_Renderer::draw_string(font, word.text, text_size, cursor + text_size * 0.5f, word.color);
                }

                cursor.x += text_size.x;
            }

            cursor.x = internal_data.left_padding;
            cursor.y -= font->char_height * 1.3f;
        }

        glm::vec2 input_text_position = { frame_buffer_size.x * 0.5f, frame_buffer_size.y * y_extent - font->char_height };
        glm::vec2 input_text_scale = { frame_buffer_size.x, 2.0f * font->char_height };
        Opengl_2D_Renderer::draw_rect(input_text_position, input_text_scale, 0.0f, internal_data.input_text_background_color);

        glm::vec2 current_text_size = font->get_string_size(internal_data.current_text);
        Opengl_2D_Renderer::draw_string(font,
                                        internal_data.current_text,
                                        current_text_size,
                                        glm::vec2(internal_data.left_padding + current_text_size.x * 0.5f - scroll_x, frame_buffer_size.y * y_extent - font->char_height),
                                        internal_data.input_text_color);

        std::string sub_string = internal_data.current_text.substr(0, internal_data.current_cursor_index);
        glm::vec2 sub_string_size = font->get_string_size(sub_string);
        glm::vec2 input_text_cursor_position = { left_padding + internal_data.input_text_cursor_size.x * 0.5f + sub_string_size.x, frame_buffer_size.y * y_extent - font->char_height };

        glm::vec4 input_text_cursor_color = internal_data.input_text_cursor_color;
        f32 &cursor_opacity = internal_data.cursor_opacity;

        if (cursor_current_cooldown_time <= 0.0f)
        {
            cursor_opacity += dt * 360.0f;
            if (cursor_opacity >= 360.0f) cursor_opacity -= 360.0f;
            input_text_cursor_color.a = glm::max(glm::abs(glm::sin(glm::radians(cursor_opacity))), internal_data.cursor_opacity_limit);
        }

        Opengl_2D_Renderer::draw_rect(input_text_cursor_position - glm::vec2(scroll_x, 0.0f), internal_data.input_text_cursor_size, 0.0f, input_text_cursor_color);

        // scroll bar
        {
            scroll_y = glm::lerp(scroll_y, scroll_y_target, dt * internal_data.scroll_speed);

            f32 console_y_size = frame_buffer_size.y * y_extent - 2.0f * font->char_height;

            bool should_render_scroll_bar = text_height > console_y_size;

            if (should_render_scroll_bar)
            {
                f32 max_scroll_y = text_height - console_y_size + font->char_height * 1.3f;
                f32 scroll_percent = (max_scroll_y - scroll_y) / max_scroll_y;

                f32 scroll_bar_y_size = glm::max((console_y_size / text_height) * console_y_size, 15.0f);
                f32 scroll_bar_y_pos  = glm::lerp(scroll_bar_y_size, console_y_size, scroll_percent);

                glm::vec2 scroll_bar_size = { 15.0f, scroll_bar_y_size };
                glm::vec2 scroll_bar_pos  = { frame_buffer_size.x - scroll_bar_size.x, scroll_bar_y_pos - scroll_bar_size.y };

                glm::vec2 scroll_rect_size = { scroll_bar_size.x, console_y_size };
                glm::vec2 scroll_rect_pos  = { frame_buffer_size.x - scroll_rect_size.x + scroll_rect_size.x * 0.5f, console_y_size * 0.5f };

                Opengl_2D_Renderer::draw_rect(scroll_rect_pos, scroll_rect_size, 0.0f, internal_data.scroll_bar_background_color);
                Opengl_2D_Renderer::draw_rect(scroll_bar_pos + scroll_bar_size * 0.5f, scroll_bar_size, 0.0f, internal_data.scroll_bar_color);
            }
        }

        // scroll_x
        {
            i32 scroll_x_count = (i32)((input_text_cursor_position.x + 3.0f * left_padding) / (frame_buffer_size.x));
            scroll_x_target = ((frame_buffer_size.x - 4.0f * left_padding) * scroll_x_count);
            scroll_x = glm::lerp(scroll_x, scroll_x_target, dt * internal_data.scroll_speed);
        }
    }

    Dropdown_Console_Data Dropdown_Console::internal_data;
}