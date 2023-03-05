#include "ui.h"
#include "core/input.h"
#include "memory/memory_arena.h"
#include "renderer/font.h"
#include "renderer/opengl_2d_renderer.h"
#include "containers/hash_table.h"

namespace minecraft {

    struct UI_Widget_State
    {
        UI_Widget *widget;

        bool clicked;

        glm::vec2 position;
        bool      dragging;
        glm::vec2 drag_offset;
    };

    u64 hash(const u64 &index)
    {
        return index;
    }

    struct UI_State
    {
        Memory_Arena          widget_arena;
        Memory_Arena          parent_arena;
        Temprary_Memory_Arena temp_widget_arena;

        Input *input;

        UI_Widget    sentinel_parent;
        u32          parent_count;
        UI_Widget  **parents;

        u32          widget_count;
        UI_Widget   *widgets;

        Hash_Table< u64, UI_Widget_State, 1024 > widget_states;

        u64 next_hot_widget;
        u64 hot_widget;
        u64 active_widget;
    };

    static UI_State *ui_state;

    bool initialize_ui(Memory_Arena *arena)
    {
        if (ui_state)
        {
            return false;
        }

        ui_state = ArenaPushAlignedZero(arena, UI_State);
        ui_state->widget_states.initialize();

        Assert(ui_state);
        ui_state->widget_arena = push_sub_arena_zero(arena, MegaBytes(1));
        ui_state->parent_arena = push_sub_arena(arena, MegaBytes(1));
        Assert(ui_state->widget_arena.base);
        Assert(ui_state->parent_arena.base);

        ui_state->parents = ArenaBeginArray(&ui_state->parent_arena, UI_Widget*);
        return true;
    }

    void shutdown_ui()
    {
    }

    static UI_Widget* current_parent()
    {
        return ui_state->parents[ui_state->parent_count - 1];
    }

    static void push_parent(UI_Widget *widget)
    {
        widget->first = widget->last = widget->next = nullptr;

        UI_Widget **parent = ArenaPushArrayEntry(&ui_state->parent_arena,
                                                  ui_state->parents);
        *parent = widget;

        ui_state->parent_count++;
    }

    static void pop_parent(UI_Widget *widget)
    {
        Assert(ui_state->parent_count);
        Assert(ui_state->parents[ui_state->parent_count - 1] == widget);
        ui_state->parent_count--;
        ui_state->parent_arena.allocated -= sizeof(UI_Widget*);
    }

    static UI_Widget*
    push_widget(u32 flags,
                const String8 &text,
                u64            hash,
                UI_Size        semantic_size[2])
    {
        UI_Widget *widget = ArenaPushArrayEntry(&ui_state->temp_widget_arena,
                                                 ui_state->widgets);

        UI_Widget *parent = current_parent();
        widget->parent    = parent;
        widget->flags     = flags;
        widget->text      = text;
        widget->hash      = hash;

        widget->semantic_size[UIAxis_X] = semantic_size[UIAxis_X];
        widget->semantic_size[UIAxis_Y] = semantic_size[UIAxis_Y];

        if (!parent->first)
        {
            parent->first = parent->last = widget;
        }
        else
        {
            parent->last->next = widget;
            parent->last       = widget;
        }

        auto it = ui_state->widget_states.find(hash);
        if (!it.entry)
        {
            UI_Widget_State empty_state = {};
            it = ui_state->widget_states.insert(hash, empty_state);
        }

        UI_Widget_State *state = it.entry;
        Assert(state);

        state->widget = widget;

        return widget;
    }

    static UI_Interaction
    handle_widget_interaction(UI_Widget *widget, Input *input)
    {
        Assert(widget);

        UI_Interaction interaction = {};
        interaction.widget = widget;

        const glm::vec2 &position = widget->position;
        const glm::vec2 &size     = widget->size;
        const glm::vec2 &mouse    = input->mouse_position;

        interaction.hovering = mouse.x >= position.x &&
                               mouse.x <= position.x + size.x &&
                               mouse.y >= position.y &&
                               mouse.y <= position.y + size.y;

        if (interaction.hovering)
        {
            ui_state->next_hot_widget = widget->hash;
            UI_Widget_State *state = ui_state->widget_states.find(widget->hash).entry;
            Assert(state);
            interaction.clicked  = state->clicked;
            interaction.dragging = state->dragging;
        }

        return interaction;
    }

    void ui_begin_frame(Input           *input,
                        const glm::vec2 &frame_buffer_size)
    {
        Assert(input);

        ui_state->temp_widget_arena = begin_temprary_memory_arena(&ui_state->widget_arena);

        ui_state->input           = input;
        ui_state->next_hot_widget = 0;
        ui_state->widgets         = ArenaBeginArray(&ui_state->temp_widget_arena, UI_Widget);

        UI_Widget *sentinel_parent = &ui_state->sentinel_parent;
        sentinel_parent->semantic_size[UIAxis_X] = { SizeKind_Pixels, frame_buffer_size.x };
        sentinel_parent->semantic_size[UIAxis_Y] = { SizeKind_Pixels, frame_buffer_size.y };

        sentinel_parent->hash  = (u64)(uintptr_t)UIName("Sentinel");
        sentinel_parent->flags = 0;
        sentinel_parent->text  = String8FromCString("Sentinel");

        sentinel_parent->cursor            = { 0.0f, 0.0f };
        sentinel_parent->relative_position = { 0.0f, 0.0f };
        sentinel_parent->position          = { 0.0f, 0.0f };
        sentinel_parent->size              = frame_buffer_size;

        auto it = ui_state->widget_states.find(sentinel_parent->hash);
        if (!it.entry)
        {
            UI_Widget_State empty_state = {};
            it = ui_state->widget_states.insert(sentinel_parent->hash, empty_state);
        }
        UI_Widget_State *state = it.entry;
        state->widget = sentinel_parent;

        push_parent(sentinel_parent);
    }

    static void traverse(UI_Widget *widget, Bitmap_Font *font)
    {
        UI_Widget *parent = widget->parent;
        Assert(parent);

        // todo(harlequin): applying universal padding for now
        glm::vec2 padding = { 5.0f, 5.0f };
        widget->cursor = padding;

        for (UI_Widget *child = widget->first;
             child;
             child = child->next)
        {
            traverse(child, font);
        }

        glm::vec2 text_size = get_string_size(font, widget->text);

        for (u32 i = 0; i < UIAxis_Count; i++)
        {
            f32      value = widget->semantic_size[i].value;
            SizeKind kind  = widget->semantic_size[i].kind;
            switch (kind)
            {
                case SizeKind_Pixels:
                {
                    widget->size[i] = value;
                } break;

                case SizeKind_TextContent:
                {
                    widget->size[i] = text_size[i] * value;
                } break;

                case SizeKind_PercentOfParent:
                {
                    Assert(parent->semantic_size[i].kind != SizeKind_ChildSum);
                    Assert(parent->semantic_size[i].kind != SizeKind_MaxChild);
                    widget->size[i] = parent->size[i] * value;
                } break;

                case SizeKind_ChildSum:
                {
                    f32 sum = 0.0f;
                    UI_Widget *child = widget->first;
                    while (child)
                    {
                        sum  += child->size[i];
                        child = child->next;
                    }
                    widget->size[i] = sum * value;
                } break;

                case SizeKind_MaxChild:
                {
                    f32 max_size = 0.0f;

                    UI_Widget *child = widget->first;
                    while (child)
                    {
                        max_size = Max(max_size, child->size[i]);
                        child    = child->next;
                    }

                    widget->size[i] = max_size * value;
                } break;

                default:
                {
                    Assert(false);
                } break;
            }
        }

        widget->size += 2.0f * padding;

        UI_Widget_State *state = ui_state->widget_states.find(widget->hash).entry;
        Assert(state);

        if (widget->flags & WidgetFlags_Draggable)
        {
            widget->relative_position = state->position;
        }
        else
        {
            widget->relative_position = parent->cursor;
            state->position = widget->relative_position;
        }

        widget->position = parent->position + widget->relative_position;
        parent->cursor.y += widget->size.y;
    }

    void ui_end_frame(Bitmap_Font *font)
    {
        pop_parent(&ui_state->sentinel_parent);
        Assert(ui_state->parent_count == 0);

        Input *input = ui_state->input;
        glm::vec2 &mouse_p = ui_state->input->mouse_position;

        if (!ui_state->sentinel_parent.first)
        {
            return;
        }

        if (ui_state->active_widget)
        {
            auto it = ui_state->widget_states.find(ui_state->active_widget);
            UI_Widget_State *active_widget_state = it.entry;
            Assert(active_widget_state);

            if (is_button_held(input, MC_MOUSE_BUTTON_LEFT))
            {
                active_widget_state->clicked = false;

                if (active_widget_state->widget->flags & WidgetFlags_Draggable)
                {
                    if (!active_widget_state->dragging)
                    {
                        active_widget_state->dragging    = true;
                        active_widget_state->drag_offset = mouse_p - active_widget_state->position;
                    }
                    else
                    {
                        glm::vec2 min_p = active_widget_state->drag_offset;
                        glm::vec2 max_p = active_widget_state->widget->parent->size - active_widget_state->widget->size + active_widget_state->drag_offset;
                        mouse_p = glm::clamp(mouse_p, min_p, max_p);
                        active_widget_state->position = mouse_p - active_widget_state->drag_offset;
                    }
                }
            }

            if (is_button_released(input, MC_MOUSE_BUTTON_LEFT))
            {
                active_widget_state->drag_offset = { 0.0f, 0.0f };
                active_widget_state->dragging    = false;
                active_widget_state->clicked     = false;
                ui_state->active_widget          = 0;
            }
        }

        if (!ui_state->active_widget)
        {
            ui_state->hot_widget = ui_state->next_hot_widget;
            if (ui_state->hot_widget)
            {
                if (is_button_pressed(input, MC_MOUSE_BUTTON_LEFT))
                {
                    ui_state->active_widget = ui_state->hot_widget;
                    ui_state->hot_widget    = 0;

                    auto it = ui_state->widget_states.find(ui_state->active_widget);
                    UI_Widget_State* active_widget_state = it.entry;
                    Assert(active_widget_state);
                    active_widget_state->clicked = true;
                }
            }
        }

        traverse(ui_state->sentinel_parent.first, font);

        ui_state->widget_count = (u32)ArenaEndArray(&ui_state->temp_widget_arena,
                                                    ui_state->widgets);

        glm::vec2 border_size = { 2.0f, 2.0f };

        glm::vec4 normal_border_color = { 1.0f, 0.0f, 0.0f, 1.0f };

        glm::vec4 normal_background_color = { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 normal_text_color = { 0.0f, 0.0f, 0.0f, 1.0f };

        glm::vec4 hot_background_color = { 0.9f, 0.9f, 0.9f, 1.0f };
        glm::vec4 hot_text_color       = { 0.1f, 0.1f, 0.1f, 1.0f };
        glm::vec4 hot_border_color     = { 0.9f, 0.0f, 0.0f, 1.0f };

        glm::vec4 active_background_color = { 0.7f, 0.7f, 0.7f, 1.0f };
        glm::vec4 active_text_color       = { 0.5f, 0.0f, 0.0f, 1.0f };
        glm::vec4 active_border_color     = { 0.5f, 0.0f, 0.0f, 1.0f };

        for (u32 i = 0; i < ui_state->widget_count; i++)
        {
            UI_Widget *widget = &ui_state->widgets[i];
            UI_Widget *parent = widget->parent;
            Assert(parent);

            glm::vec4 border_color     = normal_border_color;
            glm::vec4 background_color = normal_background_color;
            glm::vec4 text_color       = normal_text_color;

            if (widget->hash == ui_state->hot_widget)
            {
                border_color     = hot_border_color;
                background_color = hot_background_color;
                text_color       = hot_text_color;
            }
            else if (widget->hash == ui_state->active_widget)
            {
                border_color     = active_border_color;
                background_color = active_background_color;
                text_color       = active_text_color;
            }

            if (widget->flags & WidgetFlags_DrawBorder)
            {
                opengl_2d_renderer_push_quad(widget->position + widget->size * 0.5f,
                                             widget->size,
                                             0.0f,
                                             border_color);
            }

            if (widget->flags & WidgetFlags_DrawBackground)
            {
                opengl_2d_renderer_push_quad(widget->position + widget->size * 0.5f,
                                             widget->size,
                                             0.0f,
                                             background_color);
            }

            if (widget->flags & WidgetFlags_DrawText)
            {
                glm::vec2 text_size = get_string_size(font, widget->text);

                opengl_2d_renderer_push_string(font,
                                               widget->text,
                                               text_size,
                                               widget->position + widget->size * 0.5f,
                                               text_color);
            }
        }

        end_temprary_memory_arena(&ui_state->temp_widget_arena);
    }

    UI_Interaction ui_begin_panel(const char *str, u64 index)
    {
        String8 hashed_text = { (char *)str, strlen(str) };
        i32 last_hash = find_last_any_char(&hashed_text, "#");
        Assert(last_hash != -1); // invalid string format not using UIName

        String8 text = sub_str(&hashed_text, 0, last_hash - 1);

        u32 header_flags = WidgetFlags_DrawBackground|
                           WidgetFlags_Clickable|
                           WidgetFlags_Draggable;

        UI_Size semantic_size[UIAxis_Count];
        semantic_size[UIAxis_X] = { SizeKind_MaxChild, 1.0f };
        semantic_size[UIAxis_Y] = { SizeKind_ChildSum, 1.0f };

        UI_Widget *header = push_widget(header_flags,
                                        text,
                                        (u64)(uintptr_t)str * (index + 1),
                                        semantic_size);
        push_parent(header);

        u32 title_flags = WidgetFlags_DrawText;

        semantic_size[UIAxis_X] = { SizeKind_TextContent, 1.0f };
        semantic_size[UIAxis_Y] = { SizeKind_TextContent, 1.0f };

        UI_Widget *title = push_widget(title_flags,
                                       text,
                                       (u64)(uintptr_t)str * (index + 2),
                                       semantic_size);

        u32 window_flags = WidgetFlags_DrawBackground;

        semantic_size[UIAxis_X] = { SizeKind_MaxChild, 1.0f };
        semantic_size[UIAxis_Y] = { SizeKind_ChildSum, 1.0f };

        UI_Widget *window = push_widget(window_flags,
                                        text,
                                        (u64)(uintptr_t)str * (index + 3),
                                        semantic_size);

        push_parent(window);
        return handle_widget_interaction(header, ui_state->input);
    }

    void ui_end_panel()
    {
        pop_parent(current_parent());
        pop_parent(current_parent());
    }

    UI_Interaction ui_label(const char *str, u64 index)
    {
        String8 hashed_text = { (char *)str, strlen(str) };
        i32 last_hash = find_last_any_char(&hashed_text, "#");
        Assert(last_hash != -1); // invalid string format not using UIName

        String8 text = sub_str(&hashed_text, 0, last_hash - 1);
        u32 flags = WidgetFlags_DrawText;
        UI_Size semantic_size[UIAxis_Count];
        semantic_size[UIAxis_X] = { SizeKind_TextContent, 1.0f };
        semantic_size[UIAxis_Y] = { SizeKind_TextContent, 1.0f };

        UI_Widget *widget = push_widget(flags,
                                        text,
                                        (u64)(uintptr_t)str * (index + 1),
                                        semantic_size);

        return handle_widget_interaction(widget, ui_state->input);
    }

    UI_Interaction ui_button(const char *str, u64 index)
    {
        String8 hashed_text = { (char *)str, strlen(str) };
        i32 last_hash = find_last_any_char(&hashed_text, "#");
        Assert(last_hash != -1); // invalid string format not using UIName

        String8 text = sub_str(&hashed_text, 0, last_hash - 1);
        u32 flags = WidgetFlags_Clickable|
                    WidgetFlags_DrawText|
                    WidgetFlags_DrawBorder|
                    WidgetFlags_DrawBackground;

        UI_Size semantic_size[UIAxis_Count];
        semantic_size[UIAxis_X] = { SizeKind_TextContent, 1.0f };
        semantic_size[UIAxis_Y] = { SizeKind_TextContent, 1.0f };

        UI_Widget *widget = push_widget(flags,
                                        text,
                                        (u64)(uintptr_t)str * (index + 1),
                                        semantic_size);

        return handle_widget_interaction(widget, ui_state->input);
    }

#if 0

    bool UI::rect(const glm::vec2 &size)
    {
        UI_State& state = internal_data.current_state;
        glm::vec2 position = state.offset + state.cursor;
        opengl_2d_renderer_push_quad(position + size * 0.5f, size, 0.0f, state.fill_color);
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
        glm::vec2 text_size = get_string_size(state.font, text);
        opengl_2d_renderer_push_string(state.font, text, text_size, position + text_size * 0.5f, state.text_color);
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

        glm::vec2 size     = get_string_size(state.font, text);
        glm::vec2 position = state.offset + state.cursor;

        const glm::vec2& mouse = internal_data.input->mouse_position;

        bool hovered = mouse.x >= position.x && mouse.x <= position.x + size.x + padding.x &&
                       mouse.y >= position.y && mouse.y <= position.y + size.y + padding.y;

        glm::vec4 color = state.fill_color;
        if (hovered && is_button_held(internal_data.input, MC_MOUSE_BUTTON_LEFT)) color.a = 0.5f;

        opengl_2d_renderer_push_quad(position + (size + padding) * 0.5f, size + padding, 0.0f, color);
        opengl_2d_renderer_push_string(state.font, text, size, position + (size + padding) * 0.5f, state.text_color);
        state.cursor.y += size.y + padding.y + 5.0f;
        return hovered && is_button_pressed(internal_data.input, MC_MOUSE_BUTTON_LEFT);
    }

#endif
}