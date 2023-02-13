#pragma once

#include "game.h"
#include "renderer/opengl_2d_renderer.h"
#include "renderer/opengl_texture.h"
#include "renderer/font.h"
#include "core/file_system.h"

namespace minecraft {

    /*
        first slot is at 10x10
        slot size is 21x21
        1x1 padding between slots
    */

    bool initialize_inventory(Inventory   *inventory,
                              Game_Assets *assets)
    {
        for (i32 slot_index = 0; slot_index < INVENTORY_SLOT_TOTAL_COUNT; slot_index++)
        {
            Inventory_Slot& slot = inventory->slots[slot_index];
            slot.block_id = BlockId_Air;
            slot.count = 0;
        }

        inventory->active_hot_bar_slot_index = 0;
        inventory->dragging_slot_index = INVENTORY_INVALID_SLOT_INDEX;
        inventory->is_dragging = false;
        inventory->dragging_slot_offset = { 0.0f, 0.0f };

        inventory->font                  = get_font(assets->noto_mono_font);
#if 0
        inventory->blocks_sprite_sheet   = get_texture(&assets->blocks_sprite_sheet);
#else
        inventory->blocks_atlas          = &assets->blocks_atlas;
#endif
        inventory->hud_sprite            = get_texture(assets->hud_sprite);

        f32 hud_sprite_width  = (f32)inventory->hud_sprite->width;
        f32 hud_sprite_height = (f32)inventory->hud_sprite->height;

        inventory->inventory_slot_uv_rect        = convert_texture_rect_to_uv_rect({ 0, 176, 32, 32 },   hud_sprite_width, hud_sprite_height);
        inventory->active_inventory_slot_uv_rect = convert_texture_rect_to_uv_rect({ 0, 144, 32, 32 },   hud_sprite_width, hud_sprite_height);
        inventory->inventory_hud_uv_rect         = convert_texture_rect_to_uv_rect({ 276, 0, 216, 194 }, hud_sprite_width, hud_sprite_height);

        inventory->hot_bar_scale = 0.3f;

        return true;
    }

    void shutdown_inventory(Inventory *inventory, String8 path, Temprary_Memory_Arena *temp_arena)
    {
        serialize_inventory(inventory, path, temp_arena);
    }

    bool add_block_to_inventory(Inventory *inventory, u16 block_id)
    {
        bool found = false;

        for (i32 slot_index = 0; slot_index < INVENTORY_HOT_BAR_SLOT_COUNT + INVENTORY_SLOT_COUNT; slot_index++)
        {
            Inventory_Slot &slot = inventory->slots[slot_index];

            if (slot.block_id == block_id)
            {
                if (slot.count < 64)
                {
                    slot.count += 1;
                    found = true;
                    break;
                }
            }
        }

        if (!found)
        {
            for (i32 slot_index = 0;
                 slot_index < INVENTORY_HOT_BAR_SLOT_COUNT + INVENTORY_SLOT_COUNT;
                 slot_index++)
            {
                Inventory_Slot &slot = inventory->slots[slot_index];
                bool is_slot_empty   = slot.block_id == BlockId_Air && slot.count == 0;
                if (is_slot_empty)
                {
                    slot.block_id = block_id;
                    slot.count = 1;
                    found = true;
                    break;
                }
            }
        }

        return found;
    }

    void calculate_slot_positions_and_sizes(Inventory *inventory,
                                            const glm::vec2& frame_buffer_size)
    {
        f32 hot_bar_size_x = frame_buffer_size.x * inventory->hot_bar_scale;
        auto& inventory_hud_pos = inventory->inventory_hud_pos;
        auto& inventory_hud_size = inventory->inventory_hud_size;
        auto& slot_size = inventory->slot_size;
        auto& slot_padding = inventory->slot_padding;
        auto& slot_positions = inventory->slot_positions;
        inventory_hud_pos = { frame_buffer_size.x * 0.5f, frame_buffer_size.y * 0.5f };
        inventory_hud_size = { frame_buffer_size.x * 0.4f, frame_buffer_size.x * 0.4f };
        slot_size = { (21.0f / 216.0f) * inventory_hud_size.x, (21.0f / 194.0f) * inventory_hud_size.y };
        slot_padding = { (1.0f / 216.0f) * inventory_hud_size.x, (1.0f / 194.0f) * inventory_hud_size.y };
        auto half_inventory_hud_size = inventory_hud_size * 0.5f;

        f32 first_slot_x = (10.0f / 216.0f) * inventory_hud_size.x + inventory_hud_pos.x - half_inventory_hud_size.x;
        f32 first_slot_y = (10.0f / 194.0f) * inventory_hud_size.y + inventory_hud_pos.y - half_inventory_hud_size.y;

        f32 current_slot_x = first_slot_x;
        f32 current_slot_y = first_slot_y;

        i32 slot_index = INVENTORY_HOT_BAR_SLOT_COUNT + INVENTORY_SLOT_COUNT;

        for (i32 row = 0; row < 3; row++)
        {
            for (i32 col = 0; col < 3; col++)
            {
                f32 slot_x = current_slot_x + (slot_size.x + slot_padding.x) * col;
                f32 slot_y = current_slot_y + (slot_size.y + slot_padding.y) * row;
                slot_positions[slot_index++] = make_rectangle2(slot_x, slot_y, slot_size.x, slot_size.y);
            }
        }

        f32 crafting_output_slot_x = current_slot_x + (slot_size.x + slot_padding.x) * 7;
        f32 crafting_output_slot_y = current_slot_y + (slot_size.y + slot_padding.y) * 1;
        slot_positions[slot_index++] = make_rectangle2(crafting_output_slot_x, crafting_output_slot_y, slot_size.x, slot_size.y);

        // main inventory
        current_slot_y += 3.0f * (slot_size.y + slot_padding.y) + (12.0f / 194.0f) * inventory_hud_size.y;

        slot_index = INVENTORY_HOT_BAR_SLOT_COUNT;

        for (i32 row = 0; row < 3; row++)
        {
            for (i32 col = 0; col < 9; col++)
            {
                f32 slot_x = current_slot_x + (slot_size.x + slot_padding.x) * col;
                f32 slot_y = current_slot_y + (slot_size.y + slot_padding.y) * row;
                slot_positions[slot_index++] = make_rectangle2(slot_x, slot_y, slot_size.x, slot_size.y);
            }
        }

        // hot bar
        current_slot_y += 3.0f * (slot_size.y + slot_padding.y) + (9.0f / 194.0f) * inventory_hud_size.y;

        slot_index = 0;

        for (i32 row = 0; row < 1; row++)
        {
            for (i32 col = 0; col < 9; col++)
            {
                Inventory_Slot& slot = inventory->hot_bar[col];
                f32 slot_x = current_slot_x + (slot_size.x + slot_padding.x) * col;
                f32 slot_y = current_slot_y + (slot_size.y + slot_padding.y) * row;
                slot_positions[slot_index++] = make_rectangle2(slot_x, slot_y, slot_size.x, slot_size.y);
            }
        }
    }

    void handle_inventory_input(Inventory *inventory, Input *input)
    {
        const glm::vec2& mouse = input->mouse_position;

        for (i32 slot_index = 0; slot_index < INVENTORY_SLOT_TOTAL_COUNT; slot_index++)
        {
            Inventory_Slot &slot = inventory->slots[slot_index];
            const Rectangle2 &slot_rect = inventory->slot_positions[slot_index];
            bool is_slot_empty = slot.count == 0 && slot.block_id == BlockId_Air;

            if (!is_slot_empty &&
                is_point_inside_rectangle2(mouse, slot_rect) &&
                is_button_held(input, MC_MOUSE_BUTTON_LEFT) &&
                !inventory->is_dragging)
            {
                inventory->is_dragging = true;
                inventory->dragging_slot_index = slot_index;
                inventory->dragging_slot = slot;
                inventory->dragging_slot_offset = mouse - slot_rect.min;
                break;
            }
        }

        auto& inventory_hud_pos = inventory->inventory_hud_pos;
        auto& inventory_hud_size = inventory->inventory_hud_size;
        auto& slot_size = inventory->slot_size;
        auto& slot_padding = inventory->slot_padding;
        auto& slot_positions = inventory->slot_positions;
        auto half_slot_size = slot_size * 0.5f;

        if (is_button_released(input, MC_MOUSE_BUTTON_LEFT) && inventory->is_dragging)
        {
            i32 closest_slot_index = INVENTORY_INVALID_SLOT_INDEX;
            f32 best_distance_squared = Infinity32;

            for (i32 slot_index = 0; slot_index < INVENTORY_SLOT_TOTAL_COUNT; slot_index++)
            {
                Inventory_Slot &slot = inventory->slots[slot_index];
                const Rectangle2 &slot_rect = inventory->slot_positions[slot_index];

                glm::vec2 slot_center_p = slot_rect.min + half_slot_size;
                glm::vec2 mouse_to_slot = (mouse - inventory->dragging_slot_offset + half_slot_size) - slot_center_p;

                f32 distance_to_slot_squared = glm::dot(mouse_to_slot, mouse_to_slot);
                if (distance_to_slot_squared < best_distance_squared)
                {
                    best_distance_squared = distance_to_slot_squared;
                    closest_slot_index    = slot_index;
                }
            }

            if (inventory->dragging_slot_index != closest_slot_index)
            {
                Inventory_Slot& closest_slot = inventory->slots[closest_slot_index];

                if (closest_slot.block_id != inventory->dragging_slot.block_id)
                {
                    Inventory_Slot temp_slot                         = inventory->slots[closest_slot_index];
                    inventory->slots[closest_slot_index]             = inventory->dragging_slot;
                    inventory->slots[inventory->dragging_slot_index] = temp_slot;
                }
                else
                {
                    Inventory_Slot& dragging_slot = inventory->slots[inventory->dragging_slot_index];
                    i32 closest_slot_capacity     = 64 - closest_slot.count;

                    if (closest_slot_capacity >= dragging_slot.count)
                    {
                        closest_slot.count    += dragging_slot.count;
                        dragging_slot.block_id = BlockId_Air;
                        dragging_slot.count    = 0;
                    }
                    else
                    {
                        closest_slot.count   = 64;
                        dragging_slot.count -= closest_slot_capacity;
                    }
                }
            }

            inventory->is_dragging         = false;
            inventory->dragging_slot_index = INVENTORY_INVALID_SLOT_INDEX;
        }
    }

    static void draw_slot_at_index(Inventory *inventory,
                                   World *world,
                                   i32 slot_index,
                                   glm::vec2 mouse,
                                   Temprary_Memory_Arena *temp_arena)
    {
        auto& inventory_hud_pos  = inventory->inventory_hud_pos;
        auto& inventory_hud_size = inventory->inventory_hud_size;
        auto& slot_size          = inventory->slot_size;
        auto& slot_positions     = inventory->slot_positions;
        auto half_slot_size      = slot_size * 0.5f;

        Inventory_Slot &slot = inventory->slots[slot_index];
        Rectangle2 slot_rect = inventory->slot_positions[slot_index];

        glm::vec2 slot_pos = slot_rect.min;

        if (slot_index == inventory->dragging_slot_index)
        {
            slot_pos = mouse - inventory->dragging_slot_offset;
        }

        if (slot.block_id)
        {
            const Block_Info& info = world->block_infos[slot.block_id];
            UV_Rect& side_texture_uv_rect = texture_uv_rects[info.side_texture_id];
            glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

            // todo(harlequin): temparary
            if (info.flags & BlockFlags_ColorSideByBiome)
            {
                f32 rgba_color_factor = 1.0f / 255.0f;
                glm::vec4 grass_color = { 109.0f, 184.0f, 79.0f, 255.0f };
                grass_color *= rgba_color_factor;
                color = grass_color;
            }

#if 0
            opengl_2d_renderer_push_quad(slot_pos + half_slot_size,
                                         slot_size,
                                         0.0f,
                                         color,
                                         inventory->blocks_sprite_sheet,
                                         side_texture_uv_rect.top_right - side_texture_uv_rect.bottom_left,
                                         side_texture_uv_rect.bottom_left);
#else
            opengl_2d_renderer_push_quad(slot_pos + half_slot_size,
                                         slot_size,
                                         0.0f,
                                         color,
                                         inventory->blocks_atlas,
                                         info.side_texture_id);
#endif
            Bitmap_Font* font   = inventory->font;
            String8 slot_text   = push_string8(temp_arena, "%d", (u32)slot.count);
            glm::vec2 text_size = get_string_size(font, slot_text);

            opengl_2d_renderer_push_string(font,
                                           slot_text,
                                           text_size,
                                           slot_pos + half_slot_size,
                                           { 1.0f, 1.0f, 1.0f, 1.0f });
        }
    }

    void draw_inventory(Inventory *inventory, World *world, Input *input, Temprary_Memory_Arena *temp_arena)
    {
        const glm::vec2& mouse = input->mouse_position;

        auto& inventory_hud_pos  = inventory->inventory_hud_pos;
        auto& inventory_hud_size = inventory->inventory_hud_size;

        opengl_2d_renderer_push_quad(inventory_hud_pos,
                                     inventory_hud_size,
                                     0.0f,
                                     { 1.0f, 1.0f, 1.0f, 1.0f },
                                     inventory->hud_sprite,
                                     inventory->inventory_hud_uv_rect.top_right - inventory->inventory_hud_uv_rect.bottom_left,
                                     inventory->inventory_hud_uv_rect.bottom_left);

        for (i32 slot_index = 0; slot_index < INVENTORY_SLOT_TOTAL_COUNT; slot_index++)
        {
            if (slot_index != inventory->dragging_slot_index)
            {
                draw_slot_at_index(inventory, world, slot_index, mouse, temp_arena);
            }
        }

        if (inventory->dragging_slot_index != INVENTORY_INVALID_SLOT_INDEX)
        {
            draw_slot_at_index(inventory,
                               world,
                               inventory->dragging_slot_index,
                               mouse,
                               temp_arena);
        }
    }

    void handle_hotbar_input(Inventory *inventory, Input *input)
    {
        for (i32 key_code = MC_KEY_1, numpad_key_code = MC_KEY_KP_1; key_code <= MC_KEY_9; key_code++, numpad_key_code++)
        {
            if (is_key_pressed(input, key_code))
            {
                i32 slot_index = key_code - MC_KEY_1;
                inventory->active_hot_bar_slot_index = slot_index;
            }
            else if (is_key_pressed(input, numpad_key_code))
            {
                i32 slot_index = numpad_key_code - MC_KEY_KP_1;
                inventory->active_hot_bar_slot_index = slot_index;
            }
        }
    }

    void draw_hotbar(Inventory *inventory, World *world, const glm::vec2& frame_buffer_size, Temprary_Memory_Arena *temp_arena)
    {
        f32 hot_bar_size_x = frame_buffer_size.x * inventory->hot_bar_scale;

        f32 slot_width = hot_bar_size_x / INVENTORY_HOT_BAR_SLOT_COUNT;
        f32 slot_height = slot_width;
        f32 half_slot_width = slot_width * 0.5f;
        f32 half_slot_height = slot_height * 0.5f;

        f32 hot_bar_start_x = frame_buffer_size.x * 0.5f - hot_bar_size_x * 0.5f;
        f32 hot_bar_offset_from_bottom = frame_buffer_size.y - half_slot_height;

        for (i32 slot_index = 0; slot_index < INVENTORY_HOT_BAR_SLOT_COUNT; slot_index++)
        {
            Inventory_Slot& slot = inventory->hot_bar[slot_index];

            UV_Rectangle* slot_uv_rect = &inventory->inventory_slot_uv_rect;

            if (inventory->active_hot_bar_slot_index == slot_index)
            {
                slot_uv_rect = &inventory->active_inventory_slot_uv_rect;
            }

            f32 slot_center_x = hot_bar_start_x + slot_index * slot_width + half_slot_width;
            f32 slot_center_y = hot_bar_offset_from_bottom - half_slot_height;

            if (slot.block_id)
            {
                const Block_Info& info = world->block_infos[slot.block_id];
                UV_Rect& side_texture_uv_rect = texture_uv_rects[info.side_texture_id];
                glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

#if 0
                opengl_2d_renderer_push_quad({ slot_center_x, slot_center_y },
                                              { slot_width, slot_height },
                                              0.0f,
                                              color,
                                              inventory->blocks_sprite_sheet,
                                              side_texture_uv_rect.top_right - side_texture_uv_rect.bottom_left,
                                              side_texture_uv_rect.bottom_left);

#else
                opengl_2d_renderer_push_quad({ slot_center_x, slot_center_y },
                                             { slot_width, slot_height },
                                             0.0f,
                                             color,
                                             inventory->blocks_atlas,
                                             info.side_texture_id);
#endif
                Bitmap_Font* font = inventory->font;
                String8 slot_text = push_string8(temp_arena, "%d", (u32)slot.count);
                auto text_size    = get_string_size(font, slot_text);
                opengl_2d_renderer_push_string(font,
                                                slot_text,
                                                text_size,
                                                { slot_center_x, slot_center_y },
                                                { 1.0f, 1.0f, 1.0f, 1.0f });
            }

            opengl_2d_renderer_push_quad({ slot_center_x, slot_center_y },
                                          { slot_width, slot_height },
                                          0.0f,
                                          { 1.0f, 1.0f, 1.0f, 1.0f },
                                          inventory->hud_sprite,
                                          slot_uv_rect->top_right - slot_uv_rect->bottom_left,
                                          slot_uv_rect->bottom_left);
        }
    }

    void serialize_inventory(Inventory *inventory, String8 path, Temprary_Memory_Arena *temp_arena)
    {
        String8 inventory_file_path = push_string8(temp_arena,
                                                   "%.*s/inventory",
                                                   (i32)path.count,
                                                   path.data);

        FILE* file = fopen(inventory_file_path.data, "wb");
        if (!file)
        {
            fprintf(stderr,
                    "[ERROR]: failed to open file: %.*s for writing\n",
                    (i32)inventory_file_path.count,
                    inventory_file_path.data);
            return;
        }
        fwrite(inventory->slots, sizeof(inventory->slots), 1, file);
        fclose(file);
    }

    void deserialize_inventory(Inventory *inventory, String8 path, Temprary_Memory_Arena *temp_arena)
    {
        String8 inventory_file_path = push_string8(temp_arena,
                                                   "%.*s/inventory",
                                                   (i32)path.count,
                                                   path.data);

        if (!exists(inventory_file_path.data))
        {
            return;
        }

        FILE* file = fopen(inventory_file_path.data, "rb");
        if (!file)
        {
            fprintf(stderr,
                    "[ERROR]: failed to open file: %.*s for reading\n",
                    (i32)inventory_file_path.count,
                    inventory_file_path.data);
            return;
        }
        fread(inventory->slots, sizeof(inventory->slots), 1, file);
        fclose(file);
    }
}