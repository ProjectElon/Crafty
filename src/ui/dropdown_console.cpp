#include "dropdown_console.h"
#include "core/event.h"
#include "renderer/opengl_2d_renderer.h"
#include "renderer/opengl_renderer.h"
#include "renderer/font.h"
#include "game/console_commands.h"
#include "core/input.h"
#include "core/file_system.h"
#include "assets/texture_packer.h"

#include <glm/gtx/compatibility.hpp>

namespace minecraft {

    bool on_key(const Event *event, void *sender);
    bool on_mouse_wheel(const Event *event, void *sender);
    bool on_char_input(const Event *event, void *sender);

    static f32 get_line_height(Dropdown_Console *console)
    {
        Bitmap_Font* font = console->font;
        return font->char_height * 1.3f;
    }

    static f32 get_text_height(Dropdown_Console *console)
    {
        return console->line_count * get_line_height(console);
    }

    static f32 get_size_y(Dropdown_Console *console)
    {
        Bitmap_Font *font = console->font;
        glm::vec2 frame_buffer_size = opengl_renderer_get_frame_buffer_size();
        return frame_buffer_size.y * console->y_extent - 2.0f * get_line_height(console);
    }

    static f32 get_max_scroll_y(Dropdown_Console *console)
    {
        f32 text_height = get_text_height(console);

        f32 max_scroll_y   = 0.0f;
        f32 console_size_y = get_size_y(console);

        if (text_height > console_size_y)
        {
            max_scroll_y = text_height - console_size_y;
        }

        return max_scroll_y;
    }

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
                                     const glm::vec4  &command_failed_color)
    {

        console->state              = ConsoleState_Closed;
        console->string_input_arena = push_sub_arena_zero(arena, MegaBytes(1));
        console->string_arena       = push_sub_arena_zero(arena, MegaBytes(1));
        console->line_arena         = push_sub_arena_zero(arena, MegaBytes(1));
        console->font               = font;

        new (&console->push_line_mutex) std::mutex;

        console->text_color       = text_color;
        console->background_color = background_color;

        console->input_text_color            = input_text_color;
        console->input_text_background_color = input_text_background_color;

        console->scroll_bar_background_color = scroll_bar_background_color;
        console->scroll_bar_color            = scroll_bar_color;

        console->command_succeeded_color = command_succeeded_color;
        console->command_failed_color    = command_failed_color;

        console->input_text_cursor_color      = input_text_cursor_color;
        console->cursor_current_cooldown_time = 0.0f;
        console->cursor_cooldown_time         = 1.0f;
        console->input_text_cursor_size       = { font->size_in_pixels / 2.0f, font->char_height * 1.4f };
        console->cursor_opacity_limit         = 0.7f;
        console->cursor_opacity               = 0.0f;

        console->current_cursor_index = 0;
        console->current_text         = {};
        console->current_text.data    = (char*)console->string_input_arena.base;
        console->current_text.count   = 0;
        console->line_count           = 0;
        console->lines                = nullptr;

        console->padding_x        = (font->size_in_pixels / 2.0f);
        console->scroll_bar_width = 15.0f;
        console->y_extent         = 0.0f;
        console->y_extent_target  = 0.0f;
        console->toggle_speed     = 10.0f;

        console->scroll_speed = get_line_height(console) * 2.0f;

        console->scroll_y        = 0.0f;
        console->scroll_y_target = 0.0f;

        console->scroll_x        = 0.0f;
        console->scroll_x_target = 0.0f;

        register_event(event_system, EventType_Char,       on_char_input,  console);
        register_event(event_system, EventType_KeyPress,   on_key,         console);
        register_event(event_system, EventType_KeyHeld,    on_key,         console);
        register_event(event_system, EventType_MouseWheel, on_mouse_wheel, console);

        return true;
    }

    void shutdown_dropdown_console(Dropdown_Console *console)
    {
    }

    void toggle_dropdown_console(Dropdown_Console *console)
    {
        ConsoleState& state  = console->state;
        f32& y_extent_target = console->y_extent_target;

        if (state == ConsoleState_Closed)
        {
            state           = ConsoleState_HalfOpen;
            y_extent_target = 0.5f;
        }
        else if (state == ConsoleState_HalfOpen)
        {
            state           = ConsoleState_FullOpen;
            y_extent_target = 1.0f;
        }
        else if (state == ConsoleState_FullOpen)
        {
            state           = ConsoleState_Closed;
            y_extent_target = 0.0f;
        }
    }

    void clear_dropdown_console(Dropdown_Console *console)
    {
        console->line_count = 0;
        console->lines      = nullptr;
        reset_memory_arena(&console->string_arena);
        reset_memory_arena(&console->line_arena);
    }

    void open_dropdown_console_with_half_extent(Dropdown_Console *console)
    {
        auto& state           = console->state;
        auto& y_extent_target = console->y_extent_target;

        if (state == ConsoleState_Closed)
        {
            state           = ConsoleState_HalfOpen;
            y_extent_target = 0.5f;
        }
    }

    void open_dropdown_console_with_full_extent(Dropdown_Console *console)
    {
        auto& state           = console->state;
        auto& y_extent_target = console->y_extent_target;

        if (state == ConsoleState_Closed)
        {
            state           = ConsoleState_FullOpen;
            y_extent_target = 1.0f;
        }
    }

    void close_dropdown_console(Dropdown_Console *console)
    {
        auto& state           = console->state;
        auto& y_extent_target = console->y_extent_target;

        if (state == ConsoleState_HalfOpen || state == ConsoleState_FullOpen)
        {
            state           = ConsoleState_Closed;
            y_extent_target = 0.0f;
        }
    }

    Dropdown_Console_Line_Info& push_line(Dropdown_Console *console,
                                          String8 line /*= Str8("")*/,
                                          bool is_command /*= false*/,
                                          bool is_command_succeeded /*= false*/)
    {
        char *str = ArenaPushArrayAligned(&console->string_arena, char, line.count);
        memcpy(str, line.data, sizeof(char) * line.count);

        Dropdown_Console_Line_Info *line_info = ArenaPushAligned(&console->line_arena,
                                                                 Dropdown_Console_Line_Info);
        line_info->str.data                   = str;
        line_info->str.count                  = line.count;
        line_info->is_command                 = is_command;
        line_info->is_command_succeeded       = is_command_succeeded;

        if (!console->lines)
        {
            console->lines = line_info;
        }

        console->line_count++;

        return *line_info;
    }

    Dropdown_Console_Line_Info& thread_safe_push_line(Dropdown_Console *console,
                                                      String8 line /*= Str8("")*/,
                                                      bool is_command /*= false*/,
                                                      bool is_command_succeeded /*= false*/)
    {
        console->push_line_mutex.lock();
        Dropdown_Console_Line_Info& line_info = push_line(console, line, is_command, is_command_succeeded);
        console->push_line_mutex.unlock();
        return line_info;
    }

    void draw_dropdown_console(Dropdown_Console *console, f32 dt)
    {
        ConsoleState& state                = console->state;
        auto& padding_x                    = console->padding_x;
        auto& y_extent                     = console->y_extent;
        auto& y_extent_target              = console->y_extent_target;
        auto& toggle_speed                 = console->toggle_speed;
        auto& cursor_current_cooldown_time = console->cursor_current_cooldown_time;
        auto *font                         = console->font;

        auto& background_color             = console->background_color;
        auto& input_text_background_color  = console->input_text_background_color;

        auto& current_text                 = console->current_text;
        auto& current_cursor_index         = console->current_cursor_index;
        auto& input_text_color             = console->input_text_color;
        auto& input_text_cursor_size       = console->input_text_cursor_size;
        auto& input_text_cursor_color      = console->input_text_cursor_color;

        auto& command_succeeded_color      = console->command_succeeded_color;
        auto& command_failed_color         = console->command_failed_color;

        auto& cursor_opacity               = console->cursor_opacity;
        auto& cursor_opacity_limit         = console->cursor_opacity_limit;

        auto& scroll_y                     = console->scroll_y;
        auto& scroll_y_target              = console->scroll_y_target;
        auto& scroll_x                     = console->scroll_x;
        auto& scroll_x_target              = console->scroll_x_target;
        auto& scroll_speed                 = console->scroll_speed;
        auto& scroll_bar_width             = console->scroll_bar_width;
        auto& scroll_bar_background_color  = console->scroll_bar_background_color;
        auto& scroll_bar_color             = console->scroll_bar_color;

        f32 line_height = get_line_height(console);

        glm::vec2 frame_buffer_size = opengl_renderer_get_frame_buffer_size();

        y_extent = glm::lerp(y_extent, y_extent_target, dt * toggle_speed);
        y_extent = glm::clamp(y_extent, 0.0f, 1.0f);

        glm::vec2 console_background_pos  = { frame_buffer_size.x * 0.5f, frame_buffer_size.y * y_extent * 0.5f };
        glm::vec2 console_background_size = { frame_buffer_size.x, frame_buffer_size.y * y_extent };
        opengl_2d_renderer_push_quad(console_background_pos,
                                     console_background_size,
                                     0.0f,
                                     background_color);

        glm::vec2 cursor = { padding_x, frame_buffer_size.y * y_extent - 2.0f * line_height + scroll_y };
        f32 text_height  = get_text_height(console);

        for (i32 i = (i32)console->line_count - 1; i >= 0; i--)
        {
            cursor.y -= line_height;
            Dropdown_Console_Line_Info *line_info = console->lines + i;
            String8 line_str = line_info->str;

            glm::vec2 text_size = get_string_size(font, line_str);

            if (cursor.y <= frame_buffer_size.y * y_extent - 2.0f * line_height)
            {
                glm::vec4 color = input_text_color;

                if (line_info->is_command)
                {
                    if (line_info->is_command_succeeded)
                    {
                        color = command_succeeded_color;
                    }
                    else
                    {
                        color = command_failed_color;
                    }
                }

                opengl_2d_renderer_push_string(font,
                                               line_str,
                                               text_size,
                                               cursor + text_size * 0.5f,
                                               color);
            }
        }

        glm::vec2 input_text_position = { frame_buffer_size.x * 0.5f, frame_buffer_size.y * y_extent - line_height };
        glm::vec2 input_text_size = { frame_buffer_size.x, 2.0f * line_height };
        opengl_2d_renderer_push_quad(input_text_position, input_text_size, 0.0f, input_text_background_color);

        glm::vec2 current_text_size        = get_string_size(font, current_text);
        glm::vec2 current_text_fixed_size  = { 2.0f * current_text.count * input_text_cursor_size.x, current_text_size.y };
        opengl_2d_renderer_push_string(font,
                                       current_text,
                                       current_text_fixed_size,
                                       glm::vec2(padding_x + current_text_fixed_size.x * 0.5f - scroll_x, frame_buffer_size.y * y_extent - line_height),
                                       input_text_color);

        String8 sub_string = {};
        sub_string.data    = current_text.data;
        sub_string.count   = current_cursor_index;
        glm::vec2 sub_string_size = get_string_size(font, sub_string);
        glm::vec2 input_text_cursor_position = { padding_x + input_text_cursor_size.x * 0.5f + sub_string_size.x, frame_buffer_size.y * y_extent - line_height };

        // cursor
        {
            if (cursor_current_cooldown_time >= 0.0f) cursor_current_cooldown_time -= dt;

            if (cursor_current_cooldown_time <= 0.0f)
            {
                cursor_opacity += dt * 360.0f;
                if (cursor_opacity >= 360.0f)
                {
                    cursor_opacity -= 360.0f;
                }

                input_text_cursor_color.a = glm::max(glm::abs(glm::sin(glm::radians(cursor_opacity))), cursor_opacity_limit);
            }

            opengl_2d_renderer_push_quad(input_text_cursor_position - glm::vec2(scroll_x, 0.0f),
                                         input_text_cursor_size,
                                         0.0f,
                                         input_text_cursor_color);
        }

        // scroll bar
        {
            scroll_y = glm::lerp(scroll_y, scroll_y_target, dt * scroll_speed);
            f32 console_size_y = get_size_y(console);

            bool should_render_scroll_bar = text_height > console_size_y;

            if (should_render_scroll_bar)
            {
                f32 max_scroll_y = get_max_scroll_y(console);
                f32 scroll_percent = (max_scroll_y - scroll_y) / max_scroll_y;

                f32 scroll_bar_y_size = glm::max((console_size_y / text_height) * console_size_y, 10.0f);
                f32 scroll_bar_y_pos  = glm::lerp(scroll_bar_y_size, console_size_y, scroll_percent);

                glm::vec2 scroll_bar_size = { scroll_bar_width, scroll_bar_y_size };
                glm::vec2 scroll_bar_pos  = { frame_buffer_size.x - scroll_bar_size.x, scroll_bar_y_pos - scroll_bar_size.y };

                glm::vec2 scroll_rect_size = { scroll_bar_size.x, console_size_y };
                glm::vec2 scroll_rect_pos  = { frame_buffer_size.x - scroll_rect_size.x + scroll_rect_size.x * 0.5f, console_size_y * 0.5f };

                opengl_2d_renderer_push_quad(scroll_rect_pos,
                                             scroll_rect_size,
                                             0.0f,
                                             scroll_bar_background_color);

                opengl_2d_renderer_push_quad(scroll_bar_pos + scroll_bar_size * 0.5f,
                                             scroll_bar_size,
                                             0.0f,
                                             scroll_bar_color);
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

    bool on_key(const Event *event, void *sender)
    {
        Dropdown_Console *console = (Dropdown_Console*)sender;

        if (console->state == ConsoleState_Closed)
        {
            return false;
        }

        auto& current_text         = console->current_text;
        auto& current_cursor_index = console->current_cursor_index;
        auto& scroll_y_target      = console->scroll_y_target;

        console->cursor_current_cooldown_time = console->cursor_cooldown_time;
        console->cursor_opacity               = 180.0f;

        u16 key;
        parse_key_code(event, &key);

        if (key == MC_KEY_F1 || key == MC_KEY_ESCAPE)
        {
            toggle_dropdown_console(console);
        }
        else if (key == MC_KEY_ENTER || key == MC_KEY_KP_ENTER)
        {
            scroll_y_target = 0.0f;

            if (!current_text.count)
            {
                push_line(console, String8FromCString(""));
            }
            else
            {
                bool is_command = true;
                Dropdown_Console_Line_Info &info = push_line(console, current_text, is_command);
                ConsoleCommandExecutionResult result = console_commands_execute_command(current_text);
                switch (result)
                {
                    case ConsoleCommandExecutionResult_None:
                    {
                        // empty string
                    } break;

                    case ConsoleCommandExecutionResult_CommandNotFound:
                    {
                        push_line(console, String8FromCString("Command Not Found"));
                    } break;

                    case ConsoleCommandExecutionResult_ArgumentMismatch:
                    {
                        push_line(console, String8FromCString("Argument Mismatch"));
                    } break;

                    case ConsoleCommandExecutionResult_Error:
                    {
                        info.is_command_succeeded = false;
                    } break;

                    case ConsoleCommandExecutionResult_Success:
                    {
                        info.is_command_succeeded = true;
                    } break;
                }
            }

            current_text.count   = 0;
            current_cursor_index = 0;
        }
        else if (key == MC_KEY_BACKSPACE || key == MC_KEY_DELETE)
        {
            if (current_cursor_index >= 1)
            {
                for (i32 i = current_cursor_index - 1; i < (i32)current_text.count - 1; i++)
                {
                    current_text.data[i] = current_text.data[i + 1];
                }
                current_cursor_index--;
                current_text.count--;
            }
        }
        else if (key == MC_KEY_LEFT)
        {
            if (current_cursor_index >= 1)
            {
                current_cursor_index--;
            }
        }
        else if (key == MC_KEY_RIGHT)
        {
            if (current_cursor_index < current_text.count)
            {
                current_cursor_index++;
            }
        }

        return true;
    }

    bool on_mouse_wheel(const Event *event, void *sender)
    {
        Dropdown_Console *console = (Dropdown_Console*)sender;

        if (console->state == ConsoleState_Closed)
        {
            return false;
        }

        f32 x_offset;
        f32 y_offset;
        parse_mouse_wheel(event, &x_offset, &y_offset);

        glm::vec2 frame_buffer_size = opengl_renderer_get_frame_buffer_size();

        Bitmap_Font* font     = console->font;
        auto& y_extent        = console->y_extent;
        auto& scroll_y_target = console->scroll_y_target;
        auto& scroll_speed    = console->scroll_speed;

        f32 max_scroll_y = get_max_scroll_y(console);
        scroll_y_target += y_offset * scroll_speed;
        scroll_y_target  = glm::clamp(scroll_y_target, 0.0f, max_scroll_y);

        return true;
    }

    bool on_char_input(const Event *event, void *sender)
    {
        Dropdown_Console *console = (Dropdown_Console*)sender;

        if (console->state == ConsoleState_Closed)
        {
            return false;
        }

        auto& current_text         = console->current_text;
        auto& current_cursor_index = console->current_cursor_index;
        auto& scroll_y_target      = console->scroll_y_target;


        char code_point;
        parse_char(event, &code_point);
        for (i32 i = (i32)current_text.count - 1; i >= current_cursor_index; i--)
        {
            current_text.data[i + 1] = current_text.data[i];
        }
        current_text.data[current_cursor_index] = code_point;
        current_cursor_index++;
        current_text.count++;

        return true;
    }
}