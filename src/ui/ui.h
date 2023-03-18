#pragma once

#include "core/common.h"
#include "containers/string.h"
#include <glm/glm.hpp>

struct GLFWwindow;

namespace minecraft {

    struct Bitmap_Font;
    struct Opengl_Texture;
    struct Input;
    struct Memory_Arena;
    struct Temprary_Memory_Arena;

    enum WidgetFlags : u32
    {
        WidgetFlags_Clickable         = (1 << 0),
        WidgetFlags_DrawText          = (1 << 1),
        WidgetFlags_DrawBorder        = (1 << 2),
        WidgetFlags_DrawBackground    = (1 << 3),
        WidgetFlags_Draggable         = (1 << 4),
        WidgetFlags_StackVertically   = (1 << 5),
        WidgetFlags_StackHorizontally = (1 << 6),
    };

    enum SizeKind
    {
        SizeKind_Pixels,
        SizeKind_TextContent,
        SizeKind_PercentOfParent,
        SizeKind_ChildSum,
        SizeKind_MaxChild
    };

    enum UIAxis
    {
        UIAxis_X,
        UIAxis_Y,
        UIAxis_Count
    };

    struct UI_Size
    {
        SizeKind kind;
        f32      value;
    };

    enum StyleVar : u8
    {
        StyleVar_Padding,
        StyleVar_Border,
        StyleVar_BorderColor,
        StyleVar_BackgroundColor,
        StyleVar_TextColor,
        StyleVar_HotBorderColor,
        StyleVar_HotBackgroundColor,
        StyleVar_HotTextColor,
        StyleVar_ActiveBorderColor,
        StyleVar_ActiveBackgroundColor,
        StyleVar_ActiveTextColor,
        StyleVar_Count
    };

    struct UI_Widget
    {
        UI_Widget *parent;
        UI_Widget *first;
        UI_Widget *last;
        UI_Widget *next;

        glm::vec4 style_vars[StyleVar_Count];

        u64 hash;
        u32 flags;

        String8         text;
        Opengl_Texture *texture; // todo(harlequin): right now we are using Opengl_Texture
        UI_Size         semantic_size[UIAxis_Count];

        glm::vec2 cursor;
        glm::vec2 position;
        glm::vec2 size;
    };

    struct UI_Interaction
    {
        UI_Widget *widget;
        bool       hovering;
        bool       clicked;
        bool       dragging;
    };

    bool initialize_ui(Memory_Arena *arena);
    void shutdown_ui();

    void ui_begin_frame(Input *input,
                        const glm::vec2 &frame_buffer_size);

    void ui_end_frame(Bitmap_Font *font);

#define _Stringfiy(S) #S
#define Stringfiy(S) _Stringfiy(S)
#define UIName(Name) (Name "#" __FUNCTION__ Stringfiy(__LINE__) Stringfiy(__COUNTER__))
#define HashUIName(Name) u64((uintptr_t)UIName(Name))

    UI_Interaction ui_begin_panel(const char *str);
    void ui_end_panel();

    void ui_push_style(StyleVar style_variable, float value);
    void ui_push_style(StyleVar style_variable, const glm::vec2& value);
    void ui_push_style(StyleVar style_variable, const glm::vec3& value);
    void ui_push_style(StyleVar style_variable, const glm::vec4& value);
    void ui_pop_style(StyleVar style_variable);

    UI_Interaction ui_label(const char *str, const String8& text);
    UI_Interaction ui_button(const char *str);
    UI_Interaction ui_image(const char *str,
                            Opengl_Texture *texture,
                            glm::vec2 scale = { 1.0f, 1.0f });

    UI_Interaction ui_slider(const char            *str,
                             float                 *value,
                             float                  min_value = 0.0f,
                             float                  max_value = 1.0f,
                             Temprary_Memory_Arena *temp_arena = nullptr);

    UI_Interaction ui_toggle(const char *str,
                             bool       *value);
}