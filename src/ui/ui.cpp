#include "ui.h"
#include "core/input.h"
#include "renderer/font.h"
#include "renderer/opengl_2d_renderer.h"

namespace minecraft {

    bool UI::initialize(UI_State *default_state)
    {
        internal_data.default_state = *default_state;
        internal_data.current_state = *default_state;
        return true;
    }

    void UI::shutdown()
    {
    }

    void UI::begin(Input *input)
    {
        UI_State& state = internal_data.current_state;
        state = internal_data.default_state;
        internal_data.input = input;
    }

    void UI::set_font(Bitmap_Font *font)
    {
        UI_State& state = internal_data.current_state;
        state.font = font;
    }

    void UI::set_cursor(const glm::vec2& cursor)
    {
        UI_State& state = internal_data.current_state;
        state.cursor = cursor;
    }

    void UI::set_offset(const glm::vec2& offset)
    {
        UI_State& state = internal_data.current_state;
        state.offset = offset;
    }

    void UI::set_text_color(const glm::vec4& color)
    {
        UI_State& state = internal_data.current_state;
        state.text_color = color;
    }

    void UI::set_fill_color(const glm::vec4& color)
    {
        UI_State& state = internal_data.current_state;
        state.fill_color = color;
    }

    bool UI::rect(const glm::vec2& size)
    {
        UI_State& state = internal_data.current_state;
        glm::vec2 position = state.offset + state.cursor;
        Opengl_2D_Renderer::draw_rect(position + size * 0.5f, size, 0.0f, state.fill_color);
        state.cursor.y += size.y;
        const glm::vec2& mouse = internal_data.input->mouse_position;

        if (mouse.x >= position.x && mouse.x <= position.x + size.x &&
            mouse.y >= position.y && mouse.y <= position.y + size.y &&
            is_button_pressed(internal_data.input, MC_MOUSE_BUTTON_LEFT))
        {
            return true;
        }

        return false;
    }

    bool UI::text(String8 text)
    {
        UI_State& state = internal_data.current_state;
        glm::vec2 position = state.offset + state.cursor;
        glm::vec2 text_size = state.font->get_string_size(text);
        Opengl_2D_Renderer::draw_string(state.font, text, text_size, position + text_size * 0.5f, state.text_color);
        state.cursor.y += state.font->char_height * 1.3f;

        const glm::vec2& mouse = internal_data.input->mouse_position;

        if (mouse.x >= position.x && mouse.x <= position.x + text_size.x &&
            mouse.y >= position.y && mouse.y <= position.y + text_size.y &&
            is_button_pressed(internal_data.input, MC_MOUSE_BUTTON_LEFT))
        {
            return true;
        }

        return false;
    }

    bool UI::button(String8 text, const glm::vec2& padding)
    {
        UI_State& state = internal_data.current_state;

        glm::vec2 size     = state.font->get_string_size(text);
        glm::vec2 position = state.offset + state.cursor;

        const glm::vec2& mouse = internal_data.input->mouse_position;

        bool hovered = mouse.x >= position.x && mouse.x <= position.x + size.x + padding.x &&
                       mouse.y >= position.y && mouse.y <= position.y + size.y + padding.y;

        glm::vec4 color = state.fill_color;
        if (hovered && is_button_held(internal_data.input, MC_MOUSE_BUTTON_LEFT)) color.a = 0.5f;

        Opengl_2D_Renderer::draw_rect(position + (size + padding) * 0.5f, size + padding, 0.0f, color);
        Opengl_2D_Renderer::draw_string(state.font, text, size, position + (size + padding) * 0.5f, state.text_color);
        state.cursor.y += size.y + padding.y + 5.0f;
        return hovered && is_button_pressed(internal_data.input, MC_MOUSE_BUTTON_LEFT);
    }

    bool UI::textured_button(String8 text,
                             Opengl_Texture *texture,
                             const glm::vec2& padding,
                             const glm::vec2& uv_scale,
                             const glm::vec2& uv_offset)
    {
        UI_State& state = internal_data.current_state;

        glm::vec2 size     = state.font->get_string_size(text);
        glm::vec2 position = state.offset + state.cursor;

        const glm::vec2& mouse = internal_data.input->mouse_position;

        bool hovered = mouse.x >= position.x && mouse.x <= position.x + size.x + padding.x &&
                       mouse.y >= position.y && mouse.y <= position.y + size.y + padding.y;

        glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        if (hovered && is_button_held(internal_data.input, MC_MOUSE_BUTTON_LEFT)) color.a = 0.5f;

        Opengl_2D_Renderer::draw_rect(position + (size + padding) * 0.5f, size + padding, 0.0f, color, texture, uv_scale, uv_offset);
        Opengl_2D_Renderer::draw_string(state.font, text, size, position + (size + padding) * 0.5f, state.text_color);

        state.cursor.y += size.y + padding.y + 5.0f;
        return hovered && is_button_pressed(internal_data.input, MC_MOUSE_BUTTON_LEFT);
    }

    void UI::end()
    {
        UI_State& state = internal_data.current_state;
        state = internal_data.default_state;
    }

    UI_Data UI::internal_data;
}