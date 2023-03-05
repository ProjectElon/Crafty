#pragma once

#include "core/common.h"
#include "containers/string.h"
#include <glm/glm.hpp>

namespace minecraft {

    struct Bitmap_Font;
    struct Opengl_Texture;
    struct Input;
    struct Memory_Arena;

    enum WidgetFlags : u32
    {
        WidgetFlags_Clickable      = (1 << 0),
        WidgetFlags_DrawText       = (1 << 1),
        WidgetFlags_DrawBorder     = (1 << 2),
        WidgetFlags_DrawBackground = (1 << 3),
        WidgetFlags_Draggable      = (1 << 4)
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

    struct UI_Widget
    {
        UI_Widget *parent;
        UI_Widget *first;
        UI_Widget *last;
        UI_Widget *next;

        UI_Size   semantic_size[UIAxis_Count];
        u32       flags;
        String8   text;

        u64 hash;

        glm::vec2 cursor;
        glm::vec2 relative_position;
        glm::vec2 position;
        glm::vec2 size;
    };

    struct UI_Interaction
    {
        UI_Widget *widget;
        bool       clicked;
        bool       hovering;
        bool       dragging;
    };

    bool initialize_ui(Memory_Arena *arena);
    void shutdown_ui();

    void ui_begin_frame(Input *input, const glm::vec2 &frame_buffer_size);
    void ui_end_frame(Bitmap_Font *font);

#define _Stringfiy(S) #S
#define Stringfiy(S) _Stringfiy(S)
#define UIName(Name) (Name "#" __FUNCTION__ Stringfiy(__LINE__) Stringfiy(__COUNTER__))

    // note(harlequin): each one should be called with UIName for now
    UI_Interaction ui_begin_panel(const char *str,  u64 index = 0);
    void ui_end_panel();

    UI_Interaction ui_label(const char *str,  u64 index = 0);
    UI_Interaction ui_button(const char *str, u64 index = 0);
}