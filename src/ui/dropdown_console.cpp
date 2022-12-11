#include "dropdown_console.h"
#include "renderer/opengl_2d_renderer.h"
#include "renderer/opengl_renderer.h"
#include "renderer/font.h"
#include "core/input.h"

#include <glm/gtx/compatibility.hpp>

namespace minecraft {

    bool Dropdown_Console::initialize(Bitmap_Font *font,
                                      Event_System *event_system,
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
        internal_data.padding_x = (font->size_in_pixels / 2.0f);

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

        internal_data.scroll_bar_width = 15.0f;
        internal_data.y_extent = 0.0f;
        internal_data.y_extent_target = 0.0f;
        internal_data.toggle_speed = 10.0f;

        internal_data.scroll_speed = get_line_height() * 2.0f;

        internal_data.scroll_y = 0.0f;
        internal_data.scroll_y_target = 0.0f;

        internal_data.scroll_x = 0.0f;
        internal_data.scroll_x_target = 0.0f;

        register_event(event_system, EventType_Char,       Dropdown_Console::on_char_input);
        register_event(event_system, EventType_KeyPress,   Dropdown_Console::on_key);
        register_event(event_system, EventType_KeyHeld,    Dropdown_Console::on_key);
        register_event(event_system, EventType_MouseWheel, Dropdown_Console::on_mouse_wheel);

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
            parse_char(event, &code_point);
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
            parse_key_code(event, &key);

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
        parse_mouse_wheel(event, &x_offset, &y_offset);

        glm::vec2 frame_buffer_size = Opengl_Renderer::get_frame_buffer_size();

        auto& history = internal_data.history;
        auto* font = internal_data.font;
        auto& y_extent = internal_data.y_extent;
        auto& scroll_y_target = internal_data.scroll_y_target;
        auto& scroll_speed = internal_data.scroll_speed;

        f32 max_scroll_y = get_max_scroll_y();
        scroll_y_target += y_offset * scroll_speed;
        scroll_y_target = glm::clamp(scroll_y_target, 0.0f, max_scroll_y);

        return true;
    }

    f32 Dropdown_Console::get_text_height()
    {
        auto& history = internal_data.history;
        auto* font = internal_data.font;
        return history.size() * get_line_height();
    }

    f32 Dropdown_Console::get_line_height()
    {
        auto* font = internal_data.font;
        return font->char_height * 1.3f;
    }

    f32 Dropdown_Console::get_size_y()
    {
        auto& y_extent = internal_data.y_extent;
        auto* font = internal_data.font;
        glm::vec2 frame_buffer_size = Opengl_Renderer::get_frame_buffer_size();
        return frame_buffer_size.y * y_extent - 2.0f * get_line_height();
    }

    f32 Dropdown_Console::get_max_scroll_y()
    {
        f32 text_height = get_text_height();

        f32 max_scroll_y = 0.0f;
        f32 console_size_y = get_size_y();

        if (text_height > console_size_y)
        {
            max_scroll_y = text_height - console_size_y;
        }

        return max_scroll_y;
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
        auto& padding_x = internal_data.padding_x;
        auto& y_extent = internal_data.y_extent;
        auto& y_extent_target = internal_data.y_extent_target;
        auto& toggle_speed = internal_data.toggle_speed;
        auto& cursor_current_cooldown_time = internal_data.cursor_current_cooldown_time;
        auto *font = internal_data.font;
        auto& history = internal_data.history;


        auto& background_color = internal_data.background_color;

        auto& input_text_background_color = internal_data.input_text_background_color;

        auto& current_cursor_index = internal_data.current_cursor_index;
        auto& current_text = internal_data.current_text;
        auto& input_text_color = internal_data.input_text_color;
        auto& input_text_cursor_size = internal_data.input_text_cursor_size;
        auto& input_text_cursor_color = internal_data.input_text_cursor_color;

        auto& cursor_opacity = internal_data.cursor_opacity;
        auto& cursor_opacity_limit = internal_data.cursor_opacity_limit;

        auto& scroll_y = internal_data.scroll_y;
        auto& scroll_y_target = internal_data.scroll_y_target;
        auto& scroll_x = internal_data.scroll_x;
        auto& scroll_x_target = internal_data.scroll_x_target;
        auto& scroll_speed = internal_data.scroll_speed;
        auto& scroll_bar_width = internal_data.scroll_bar_width;
        auto& scroll_bar_background_color = internal_data.scroll_bar_background_color;
        auto& scroll_bar_color = internal_data.scroll_bar_color;

        f32 line_height = get_line_height();

        glm::vec2 frame_buffer_size = Opengl_Renderer::get_frame_buffer_size();

        y_extent = glm::lerp(y_extent, y_extent_target, dt * toggle_speed);
        y_extent = glm::clamp(y_extent, 0.0f, 1.0f);

        glm::vec2 console_background_pos = { frame_buffer_size.x * 0.5f, frame_buffer_size.y * y_extent * 0.5f };
        glm::vec2 console_background_size = { frame_buffer_size.x, frame_buffer_size.y * y_extent };
        Opengl_2D_Renderer::draw_rect(console_background_pos, console_background_size, 0.0f, background_color);

        glm::vec2 cursor = { padding_x, frame_buffer_size.y * y_extent - 2.0f * line_height + scroll_y };

        i32 count = (i32)history.size();
        f32 text_height = get_text_height();

        for (i32 i = count - 1; i >= 0; i--)
        {
            const Dropdown_Console_Line& line = history[i];

            for (i32 j = 0; j < (i32)line.words.size(); ++j)
            {
                const Dropdown_Console_Word& word = line.words[j];

                glm::vec2 text_size = font->get_string_size(word.text);

                if (cursor.y <= frame_buffer_size.y * y_extent - 2.0f * line_height)
                {
                    Opengl_2D_Renderer::draw_string(font, word.text, text_size, cursor + text_size * 0.5f, word.color);
                }

                cursor.x += text_size.x;
            }

            cursor.x = padding_x;
            cursor.y -= line_height;
        }

        glm::vec2 input_text_position = { frame_buffer_size.x * 0.5f, frame_buffer_size.y * y_extent - line_height };
        glm::vec2 input_text_size = { frame_buffer_size.x, 2.0f * line_height };
        Opengl_2D_Renderer::draw_rect(input_text_position, input_text_size, 0.0f, input_text_background_color);

        glm::vec2 current_text_size = font->get_string_size(current_text);
        Opengl_2D_Renderer::draw_string(font,
                                        current_text,
                                        current_text_size,
                                        glm::vec2(padding_x + current_text_size.x * 0.5f - scroll_x, frame_buffer_size.y * y_extent - line_height),
                                        input_text_color);

        std::string sub_string = current_text.substr(0, current_cursor_index);
        glm::vec2 sub_string_size = font->get_string_size(sub_string);
        glm::vec2 input_text_cursor_position = { padding_x + input_text_cursor_size.x * 0.5f + sub_string_size.x, frame_buffer_size.y * y_extent - line_height };

        // cursor
        {
            if (cursor_current_cooldown_time >= 0.0f) cursor_current_cooldown_time -= dt;

            if (cursor_current_cooldown_time <= 0.0f)
            {
                cursor_opacity += dt * 360.0f;
                if (cursor_opacity >= 360.0f) cursor_opacity -= 360.0f;
                input_text_cursor_color.a = glm::max(glm::abs(glm::sin(glm::radians(cursor_opacity))), cursor_opacity_limit);
            }

            Opengl_2D_Renderer::draw_rect(input_text_cursor_position - glm::vec2(scroll_x, 0.0f), input_text_cursor_size, 0.0f, input_text_cursor_color);
        }

        // scroll bar
        {
            scroll_y = glm::lerp(scroll_y, scroll_y_target, dt * scroll_speed);
            f32 console_size_y = get_size_y();

            bool should_render_scroll_bar = text_height > console_size_y;

            if (should_render_scroll_bar)
            {
                f32 max_scroll_y = get_max_scroll_y();
                f32 scroll_percent = (max_scroll_y - scroll_y) / max_scroll_y;

                f32 scroll_bar_y_size = glm::max((console_size_y / text_height) * console_size_y, 10.0f);
                f32 scroll_bar_y_pos  = glm::lerp(scroll_bar_y_size, console_size_y, scroll_percent);

                glm::vec2 scroll_bar_size = { scroll_bar_width, scroll_bar_y_size };
                glm::vec2 scroll_bar_pos  = { frame_buffer_size.x - scroll_bar_size.x, scroll_bar_y_pos - scroll_bar_size.y };

                glm::vec2 scroll_rect_size = { scroll_bar_size.x, console_size_y };
                glm::vec2 scroll_rect_pos  = { frame_buffer_size.x - scroll_rect_size.x + scroll_rect_size.x * 0.5f, console_size_y * 0.5f };

                Opengl_2D_Renderer::draw_rect(scroll_rect_pos, scroll_rect_size, 0.0f, scroll_bar_background_color);
                Opengl_2D_Renderer::draw_rect(scroll_bar_pos + scroll_bar_size * 0.5f, scroll_bar_size, 0.0f, scroll_bar_color);
            }
        }

        // input_text scroll
        {
            scroll_x_target = 0.0f;

            if (current_text_size.x + 2.0f * input_text_cursor_size.x + padding_x > input_text_size.x)
            {
                scroll_x_target = current_text_size.x + 2.0f * input_text_cursor_size.x + padding_x - input_text_size.x;
            }

            scroll_x = glm::lerp(scroll_x, scroll_x_target, dt * scroll_speed);
        }
    }

    Dropdown_Console_Data Dropdown_Console::internal_data;
}