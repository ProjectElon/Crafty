#include "ui.h"
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

    void UI::begin()
    {
        UI_State& state = internal_data.current_state;
        state = internal_data.default_state;
    }

    void UI::set_font(Bitmap_Font *font)
    {
        UI_State& state = internal_data.current_state;
        state.font = font;
    }

    void UI::set_color(const glm::vec4& color)
    {
        UI_State& state = internal_data.current_state;
        state.color = color;
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

    bool UI::rect(const glm::vec2& size)
    {
        UI_State& state = internal_data.current_state;
        Opengl_2D_Renderer::draw_rect(state.offset + state.cursor + size * 0.5f, size, 0.0f, state.color);
        state.cursor.y += size.y;
        return false;
    }

    bool UI::text(const std::string& text)
    {
        UI_State& state = internal_data.current_state;
        glm::vec2 text_size = state.font->get_string_size(text);
        Opengl_2D_Renderer::draw_string(state.font, text, text_size, state.offset + state.cursor + 0.5f * text_size, state.color);
        state.cursor.y += text_size.y;
        return false;
    }

    void UI::end()
    {
        UI_State& state = internal_data.current_state;
        state = internal_data.default_state;
    }

    UI_Data UI::internal_data;
}