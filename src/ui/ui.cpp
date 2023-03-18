#include "ui.h"
#include "core/input.h"
#include "memory/memory_arena.h"
#include "renderer/font.h"
#include "renderer/opengl_2d_renderer.h"
#include "containers/hash_table.h"
#include <GLFW/glfw3.h>

namespace minecraft {

    struct UI_Widget_State
    {
        UI_Widget *widget;

        glm::vec2 relative_position;
        glm::vec2 position;
        glm::vec2 size;

        bool clicked;

        bool      is_initial_dragging_position_set;
        bool      dragging;
        bool      drag_constraint_x;
        bool      drag_constraint_y;
        glm::vec2 drag_offset;
        glm::vec2 drag_mouse_p;
    };

    UI_Widget_State
    make_default_ui_widget_state()
    {
        UI_Widget_State result;
        result.widget = nullptr;
        result.relative_position = { 0.0f, 0.0f };
        result.position = { 0.0f, 0.0f };
        result.clicked = false;
        result.dragging = false;
        result.is_initial_dragging_position_set = false;
        result.drag_constraint_x = false;
        result.drag_constraint_y = false;
        result.drag_offset = { 0.0f, 0.0f };
        result.drag_mouse_p = { 0.0f, 0.0f };
        return result;
    }

    u64 hash(const u64 &index)
    {
        return index;
    }

    struct Style_Variable
    {
        glm::vec4       value;
        Style_Variable *next;
        Style_Variable *prev;
    };

    struct Style_Variable_List
    {
        u32             count;

        Style_Variable *first;
        Style_Variable *last;

        Style_Variable *first_free;
    };

    struct UI_State
    {
        Memory_Arena          style_arena;
        Memory_Arena          widget_arena;
        Memory_Arena          parent_arena;

        Temprary_Memory_Arena temp_widget_arena;
        Temprary_Memory_Arena temp_style_arena;

        Input *input;

        UI_Widget    sentinel_parent;
        u32          parent_count;
        UI_Widget  **parents;

        u32          widget_count;
        UI_Widget   *widgets;

        Hash_Table< u64, UI_Widget_State, 1024 > widget_states;

        Style_Variable_List style_variable_lists[StyleVar_Count];

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
        ui_state->style_arena  = push_sub_arena_zero(arena, MegaBytes(1));
        ui_state->widget_arena = push_sub_arena_zero(arena, MegaBytes(1));
        ui_state->parent_arena = push_sub_arena(arena, MegaBytes(1));
        Assert(ui_state->widget_arena.base);
        Assert(ui_state->parent_arena.base);

        ui_state->parents = ArenaBeginArray(&ui_state->parent_arena, UI_Widget*);

        Style_Variable_List *style_variable_lists = ui_state->style_variable_lists;

        for (u32 i = 0; i < StyleVar_Count; i++)
        {
            Style_Variable_List *style_variable_list = &style_variable_lists[i];
            style_variable_list->first =
            style_variable_list->last  = ArenaPushAligned(&ui_state->style_arena, Style_Variable);
        }

        style_variable_lists[StyleVar_Padding].first->value               = { 1.0f, 1.0f, 0.0f, 0.0f };
        style_variable_lists[StyleVar_Border].first->value                = { 1.0f, 1.0f, 0.0f, 0.0f };
        style_variable_lists[StyleVar_BorderColor].first->value           = { 1.0f, 0.0f, 0.0f, 1.0f };
        style_variable_lists[StyleVar_BackgroundColor].first->value       = { 1.0f, 1.0f, 1.0f, 1.0f };
        style_variable_lists[StyleVar_TextColor].first->value             = { 0.0f, 0.0f, 0.0f, 1.0f };
        style_variable_lists[StyleVar_HotBorderColor].first->value        = { 0.9f, 0.0f, 0.0f, 1.0f };
        style_variable_lists[StyleVar_HotBackgroundColor].first->value    = { 0.9f, 0.9f, 0.9f, 1.0f };
        style_variable_lists[StyleVar_HotTextColor].first->value          = { 0.1f, 0.1f, 0.1f, 1.0f };
        style_variable_lists[StyleVar_ActiveBorderColor].first->value     = { 0.5f, 0.0f, 0.0f, 1.0f };
        style_variable_lists[StyleVar_ActiveBackgroundColor].first->value = { 0.7f, 0.7f, 0.7f, 1.0f };
        style_variable_lists[StyleVar_ActiveTextColor].first->value       = { 0.5f, 0.0f, 0.0f, 1.0f };

        return true;
    }

    void shutdown_ui()
    {
    }

    void ui_push_style(StyleVar style_variable, float value)
    {
        ui_push_style(style_variable, glm::vec4(value, 0.0f, 0.0f, 0.0f));
    }

    void ui_push_style(StyleVar style_variable, const glm::vec2 &value)
    {
        ui_push_style(style_variable, glm::vec4(value, 0.0f, 0.0f));
    }

    void ui_push_style(StyleVar style_variable, const glm::vec3 &value)
    {
        ui_push_style(style_variable, glm::vec4(value, 0.0f));
    }

    void ui_push_style(StyleVar style_variable, const glm::vec4 &value)
    {
        Assert(style_variable < StyleVar_Count);

        Style_Variable_List *list = &ui_state->style_variable_lists[style_variable];

         Style_Variable *variable = nullptr;

        if (list->first_free)
        {
            variable = list->first_free;
            list->first_free->next;
        }
        else
        {
            variable = ArenaPushAlignedZero(&ui_state->temp_style_arena,
                                            Style_Variable);
        }

        variable->value = value;
        variable->next  = list->first;
        variable->prev  = list->last;

        list->first->prev = variable;
        list->last->next  = variable;
        list->last        = variable;

        list->count++;
    }

    void ui_pop_style(StyleVar style_variable)
    {
        Assert(style_variable < StyleVar_Count);
        Style_Variable_List *list = &ui_state->style_variable_lists[style_variable];
        Assert(list->count);
        Style_Variable *last = list->last;
        list->last       = list->last->prev;
        list->last->next = list->first;

        last->next       = list->first_free;
        list->first_free = last;

        list->count--;
    }

    static UI_Widget*
    get_current_parent()
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

    static void
    set_widget_style_vars(UI_Widget *widget)
    {
        for (u32 i = 0; i < StyleVar_Count; i++)
        {
            Style_Variable_List *list = &ui_state->style_variable_lists[i];
            widget->style_vars[i] = list->last->value;
        }
    }

    static UI_Widget*
    push_widget(u32            flags,
                const String8 &text,
                u64            hash,
                UI_Size        semantic_size_x,
                UI_Size        semantic_size_y)
    {
        UI_Widget *widget = ArenaPushArrayEntry(&ui_state->temp_widget_arena,
                                                 ui_state->widgets);

        UI_Widget *parent = get_current_parent();
        widget->parent    = parent;
        widget->flags     = flags;
        widget->text      = text;
        widget->hash      = hash;
        widget->texture   = nullptr;

        widget->semantic_size[UIAxis_X] = semantic_size_x;
        widget->semantic_size[UIAxis_Y] = semantic_size_y;

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
            it = ui_state->widget_states.insert(hash, make_default_ui_widget_state());
        }

        UI_Widget_State *state = it.entry;
        Assert(state);
        state->widget = widget;

        set_widget_style_vars(widget);

        return widget;
    }

    static UI_Interaction
    handle_widget_interaction(UI_Widget *widget, Input *input)
    {
        Assert(widget);

        UI_Interaction interaction = {};
        interaction.widget = widget;

        UI_Widget_State *state = ui_state->widget_states.find(widget->hash).entry;
        Assert(state);

        const glm::vec2 &position = state->position;
        const glm::vec2 &size     = state->size;
        const glm::vec2 &mouse    = input->mouse_position;

        bool hovering = mouse.x >= position.x &&
                        mouse.x <= position.x + size.x &&
                        mouse.y >= position.y &&
                        mouse.y <= position.y + size.y;
        if (hovering)
        {
            ui_state->next_hot_widget = widget->hash;
        }

        interaction.clicked  = state->clicked;
        interaction.dragging = state->dragging;

        if (ui_state->hot_widget == widget->hash)
        {
            interaction.hovering = true;
        }

        return interaction;
    }

    void ui_begin_frame(Input           *input,
                        const glm::vec2 &frame_buffer_size)
    {
        Assert(input);

        ui_state->temp_style_arena = begin_temprary_memory_arena(&ui_state->style_arena);
        ui_state->temp_widget_arena = begin_temprary_memory_arena(&ui_state->widget_arena);

        ui_state->input           = input;
        ui_state->next_hot_widget = 0;
        ui_state->widgets         = ArenaBeginArray(&ui_state->temp_widget_arena, UI_Widget);

        UI_Widget *sentinel_parent = &ui_state->sentinel_parent;
        sentinel_parent->semantic_size[UIAxis_X] = { SizeKind_Pixels, frame_buffer_size.x };
        sentinel_parent->semantic_size[UIAxis_Y] = { SizeKind_Pixels, frame_buffer_size.y };

        sentinel_parent->hash  = HashUIName("Sentinel");
        sentinel_parent->flags = WidgetFlags_StackVertically;
        sentinel_parent->text  = String8FromCString("Sentinel");

        sentinel_parent->style_vars[StyleVar_Padding] = { 0.0f, 0.0f, 0.0f, 0.0f };
        sentinel_parent->style_vars[StyleVar_Border]  = { 0.0f, 0.0f, 0.0f, 0.0f };
        sentinel_parent->cursor   = { 0.0f, 0.0f };
        sentinel_parent->position = { 0.0f, 0.0f };
        sentinel_parent->size     = frame_buffer_size;

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

        for (UI_Widget *child = widget->first;
             child;
             child = child->next)
        {
            traverse(child, font);
        }

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
                    glm::vec2 text_size = get_string_size(font, widget->text);
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

        glm::vec2 padding = widget->style_vars[StyleVar_Padding];
        glm::vec2 border = widget->style_vars[StyleVar_Border];
        widget->cursor = padding + border;
        widget->size  += 2.0f * (padding + border);
    }

    static void draw_ui(UI_Widget *widget, Bitmap_Font *font)
    {
        glm::vec4 border_color     = widget->style_vars[StyleVar_BorderColor];
        glm::vec4 background_color = widget->style_vars[StyleVar_BackgroundColor];
        glm::vec4 text_color       = widget->style_vars[StyleVar_TextColor];

        if (widget->hash == ui_state->hot_widget)
        {
            border_color     = widget->style_vars[StyleVar_HotBorderColor];
            background_color = widget->style_vars[StyleVar_HotBackgroundColor];
            text_color       = widget->style_vars[StyleVar_HotTextColor];
        }
        else if (widget->hash == ui_state->active_widget)
        {
            border_color     = widget->style_vars[StyleVar_ActiveBorderColor];
            background_color = widget->style_vars[StyleVar_ActiveBackgroundColor];
            text_color       = widget->style_vars[StyleVar_ActiveTextColor];
        }

        if (widget->flags & WidgetFlags_DrawBorder)
        {
            glm::vec2 p    = widget->position;
            glm::vec2 size = widget->size;
            opengl_2d_renderer_push_quad(widget->position + size * 0.5f,
                                         size,
                                         0.0f,
                                         border_color);
        }

        if (widget->flags & WidgetFlags_DrawBackground)
        {
            glm::vec2 border = widget->style_vars[StyleVar_Border];
            glm::vec2 p    = widget->position + border;
            glm::vec2 size = widget->size - 2.0f * border;
            opengl_2d_renderer_push_quad(p + size * 0.5f,
                                         size,
                                         0.0f,
                                         background_color,
                                         widget->texture);
        }

        if (widget->flags & WidgetFlags_DrawText)
        {
            glm::vec2 p    = widget->position;
            glm::vec2 size = widget->size;
            glm::vec2 text_size = get_string_size(font, widget->text);

            opengl_2d_renderer_push_string(font,
                                           widget->text,
                                           text_size,
                                           p + size * 0.5f,
                                           text_color);
        }

        for (UI_Widget *child = widget->first;
             child;
             child = child->next)
        {
            draw_ui(child, font);
        }
    }

    void ui_end_frame(Bitmap_Font *font)
    {
        for (u32 i = 0; i < StyleVar_Count; i++)
        {
            Style_Variable_List *list = &ui_state->style_variable_lists[i];
            Assert(list->count == 0);
        }

        pop_parent(&ui_state->sentinel_parent);
        Assert(ui_state->parent_count == 0);

        ui_state->widget_count = (u32)ArenaEndArray(&ui_state->temp_widget_arena,
                                                    ui_state->widgets);

        Input *input = ui_state->input;
        glm::vec2 &mouse_p = input->mouse_position;

        if (!ui_state->sentinel_parent.first)
        {
            return;
        }

        for (u32 i = 0; i < ui_state->widget_count; i++)
        {
            UI_Widget *widget = &ui_state->widgets[i];

            UI_Widget *parent = widget->parent;
            UI_Widget_State *state = ui_state->widget_states.find(widget->hash).entry;
            Assert(state);

            if ((widget->flags & WidgetFlags_Draggable))
            {
                if (!state->is_initial_dragging_position_set)
                {
                    state->relative_position = parent->cursor;
                    state->is_initial_dragging_position_set = true;
                }
            }
            else
            {
                state->relative_position = parent->cursor;
            }

            state->position  = parent->position + state->relative_position;
            state->size      = widget->size;
            widget->position = state->position;

            if (parent->flags & WidgetFlags_StackHorizontally)
            {
                parent->cursor.x += widget->size.x;
            }

            if (parent->flags & WidgetFlags_StackVertically)
            {
                parent->cursor.y += widget->size.y;
            }
        }

        if (ui_state->active_widget)
        {
            auto it = ui_state->widget_states.find(ui_state->active_widget);
            UI_Widget_State *active_widget_state = it.entry;
            Assert(active_widget_state);

            if (is_button_held(input, MC_MOUSE_BUTTON_LEFT))
            {
                active_widget_state->clicked = false;

                if ((active_widget_state->widget->flags & WidgetFlags_Draggable))
                {
                    if (!active_widget_state->dragging)
                    {
                        active_widget_state->dragging     = true;
                        active_widget_state->drag_mouse_p = mouse_p;
                        active_widget_state->drag_offset  = mouse_p - active_widget_state->relative_position;
                    }
                    else
                    {
                        UI_Widget *widget = active_widget_state->widget;
                        UI_Widget *parent = widget->parent;

                        glm::vec2 min_p = active_widget_state->drag_offset;
                        glm::vec2 max_p = parent->size - (widget->size - active_widget_state->drag_offset);

                        if (active_widget_state->drag_constraint_x)
                        {
                            mouse_p.x = active_widget_state->drag_mouse_p.x;
                        }

                        if (active_widget_state->drag_constraint_y)
                        {
                            mouse_p.y = active_widget_state->drag_mouse_p.y;
                        }

                        mouse_p = glm::clamp(mouse_p, min_p, max_p);
                        set_mouse_position(input, mouse_p);
                        active_widget_state->relative_position = (mouse_p - active_widget_state->drag_offset);
                    }
                }
            }

            if (is_button_released(input, MC_MOUSE_BUTTON_LEFT))
            {
                active_widget_state->clicked  = false;
                active_widget_state->dragging = false;

                active_widget_state->drag_mouse_p = { 0.0f, 0.0f };
                active_widget_state->drag_offset  = { 0.0f, 0.0f };

                ui_state->active_widget = 0;
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

        for (UI_Widget *widget = ui_state->sentinel_parent.first;
             widget;
             widget = widget->next)
        {
            traverse(widget, font);
            draw_ui(widget, font);
        }

        end_temprary_memory_arena(&ui_state->temp_widget_arena);
        end_temprary_memory_arena(&ui_state->temp_style_arena);
    }

    String8 handle_ui_string(const char *str, u64 *out_hash)
    {
        String8 text = { (char *)str, strlen(str) };
        *out_hash    = hash(text);
        i32 last_hash = find_last_any_char(&text, "#");
        if (last_hash != -1)
        {
            text = sub_str(&text, 0, last_hash - 1);
        }
        return text;
    }

    UI_Interaction ui_begin_panel(const char *str)
    {
        u64 hash;
        String8 text = handle_ui_string(str, &hash);
        UI_Widget *header = push_widget(WidgetFlags_DrawBackground|
                                        WidgetFlags_DrawBorder|
                                        WidgetFlags_Clickable|
                                        WidgetFlags_StackVertically,
                                        text,
                                        HashUIName("header"),
                                        { SizeKind_MaxChild, 1.0f },
                                        { SizeKind_ChildSum, 1.0f });
        push_parent(header);
        UI_Widget *title = push_widget(WidgetFlags_DrawText,
                                       text,
                                       HashUIName("title"),
                                       { SizeKind_TextContent, 1.0f },
                                       { SizeKind_TextContent, 1.0f });

        UI_Widget *window = push_widget(WidgetFlags_DrawBackground|
                                        WidgetFlags_StackVertically,
                                        text,
                                        HashUIName("window"),
                                        { SizeKind_MaxChild, 1.0f },
                                        { SizeKind_ChildSum, 1.0f });
        push_parent(window);
        return handle_widget_interaction(header, ui_state->input);
    }

    void ui_end_panel()
    {
        pop_parent(get_current_parent());
        pop_parent(get_current_parent());
    }

    UI_Interaction ui_label(const char *str, const String8& text)
    {
        u64 hash;
        handle_ui_string(str, &hash);

        UI_Widget *widget = push_widget(WidgetFlags_DrawText,
                                        text,
                                        hash,
                                        { SizeKind_TextContent, 1.0f },
                                        { SizeKind_TextContent, 1.0f });

        return handle_widget_interaction(widget, ui_state->input);
    }

    UI_Interaction ui_button(const char *str)
    {
        u64 hash;
        String8 text = handle_ui_string(str, &hash);

        UI_Widget *widget = push_widget(WidgetFlags_Clickable|
                                        WidgetFlags_DrawText|
                                        WidgetFlags_DrawBorder|
                                        WidgetFlags_DrawBackground,
                                        text,
                                        hash,
                                        { SizeKind_TextContent, 1.0f },
                                        { SizeKind_TextContent, 1.0f });

        return handle_widget_interaction(widget, ui_state->input);
    }

    UI_Interaction ui_image(const char     *str,
                            Opengl_Texture *texture,
                            glm::vec2       scale)
    {
        u64 hash;
        String8 text = handle_ui_string(str, &hash);

        UI_Widget *widget = push_widget(WidgetFlags_Clickable|
                                        WidgetFlags_DrawBorder|
                                        WidgetFlags_DrawBackground,
                                        text,
                                        hash,
                                        { SizeKind_Pixels, texture->width  * scale.x },
                                        { SizeKind_Pixels, texture->height * scale.y });
        widget->texture = texture;
        return handle_widget_interaction(widget, ui_state->input);
    }

    UI_Interaction ui_slider(const char            *str,
                             float                 *value,
                             float                  min_value,
                             float                  max_value,
                             Temprary_Memory_Arena *temp_arena)
    {
        u64 hash;
        String8 text = handle_ui_string(str, &hash);
        UI_Widget *spacer = push_widget(WidgetFlags_StackHorizontally,
                                        text,
                                        0,
                                        { SizeKind_ChildSum, 1.0f },
                                        { SizeKind_MaxChild, 1.0f });

        UI_Interaction slider_interaction = {};

        push_parent(spacer);
        {
            UI_Widget *parent = push_widget(WidgetFlags_Clickable|
                                            WidgetFlags_DrawBorder|
                                            WidgetFlags_DrawBackground,
                                            text,
                                            HashUIName("parent"),
                                            { SizeKind_Pixels, 300.0f },
                                            { SizeKind_Pixels, 20.0f });

            UI_Widget_State *parent_state = ui_state->widget_states.find(parent->hash).entry;

            UI_Interaction parent_interaction = handle_widget_interaction(parent, ui_state->input);

            UI_Widget_State *slider_state;

            push_parent(parent);
            {
                UI_Widget *slider = push_widget(WidgetFlags_Clickable|
                                                WidgetFlags_Draggable|
                                                WidgetFlags_DrawBorder|
                                                WidgetFlags_DrawBackground,
                                                text,
                                                hash,
                                                { SizeKind_PercentOfParent, 0.1f },
                                                { SizeKind_PercentOfParent, 1.0f });

                slider->style_vars[StyleVar_Border]  = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
                slider->style_vars[StyleVar_Padding] = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

                slider_interaction = handle_widget_interaction(slider, ui_state->input);
                slider_state = ui_state->widget_states.find(slider->hash).entry;
            }
            pop_parent(parent);

            slider_state->drag_constraint_y = true;

            *value = glm::clamp(*value, min_value, max_value);
            glm::vec2 &slider_p = slider_state->relative_position;

            f32 slider_x = slider_state->position.x;
            f32 parent_x = parent_state->position.x;
            f32 slider_size_x = slider_state->size.x;
            f32 parent_size_x = parent_state->size.x;

            if (parent_interaction.clicked)
            {
                const glm::vec2 &mouse = ui_state->input->mouse_position;
                f32 t = (mouse.x - parent_x) / (parent_size_x - slider_size_x);
                *value = min_value + (max_value - min_value) * t;
            }

            if (slider_state->dragging)
            {
                f32 t = (slider_x - parent_x) / (parent_size_x - slider_size_x);
                *value = min_value + (max_value - min_value) * t;
            }
            else
            {
                f32 t = (*value - min_value) / (max_value - min_value);
                slider_p.x = t * (parent_size_x - slider_size_x);
            }

            if (temp_arena)
            {
                String8 slider_text = push_string8(temp_arena,
                                                   "%.*s: %.2f",
                                                   (i32)text.count,
                                                   text.data,
                                                   *value);

                UI_Widget *widget   = push_widget(WidgetFlags_DrawText,
                                                  slider_text,
                                                  0,
                                                  { SizeKind_TextContent, 1.0f },
                                                  { SizeKind_TextContent, 1.0f });
            }
        }
        pop_parent(spacer);

        return slider_interaction;
    }


    UI_Interaction ui_toggle(const char *str,
                             bool       *value)
    {
        UI_Interaction interaction = {};

        u64 hash;
        String8 text = handle_ui_string(str, &hash);
        UI_Widget *spacer = push_widget(WidgetFlags_StackHorizontally,
                                        text,
                                        0,
                                        { SizeKind_ChildSum, 1.0f },
                                        { SizeKind_ChildSum, 1.0f });
        push_parent(spacer);
        {
            UI_Widget *toggle_box = push_widget(WidgetFlags_DrawBackground|
                                                WidgetFlags_DrawBorder|
                                                WidgetFlags_Clickable,
                                                text,
                                                hash,
                                                { SizeKind_Pixels, 20.0f },
                                                { SizeKind_Pixels, 20.0f });

            interaction = handle_widget_interaction(toggle_box,
                                                    ui_state->input);
            if (interaction.clicked)
            {
                *value = !(*value);
            }

            if (*value)
            {
                toggle_box->style_vars[StyleVar_BackgroundColor] = { 0.0f, 0.0f, 0.0f, 1.0f };
                toggle_box->style_vars[StyleVar_HotBackgroundColor] = { 0.0f, 0.0f, 0.0f, 1.0f };
                toggle_box->style_vars[StyleVar_ActiveBackgroundColor] = { 0.0f, 0.0f, 0.0f, 1.0f };
            }
            else
            {
                toggle_box->style_vars[StyleVar_BackgroundColor] = { 1.0f, 1.0f, 1.0f, 1.0f };
                toggle_box->style_vars[StyleVar_HotBackgroundColor] = { 1.0f, 1.0f, 1.0f, 1.0f };
                toggle_box->style_vars[StyleVar_ActiveBackgroundColor] = { 1.0f, 1.0f, 1.0f, 1.0f };
            }

            UI_Widget *label = push_widget(WidgetFlags_DrawText,
                                           text,
                                           HashUIName("label"),
                                           { SizeKind_TextContent, 1.0f },
                                           { SizeKind_TextContent, 1.0f });
        }

        pop_parent(spacer);

        return interaction;
    }
}