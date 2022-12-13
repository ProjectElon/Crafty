#pragma once

#include "game/inventory.h"
#include "game/world.h"
#include "core/input.h"
#include "renderer/opengl_renderer.h"
#include "renderer/opengl_2d_renderer.h"
#include "renderer/font.h"

#include <filesystem>
#include <sstream>

namespace minecraft {

    /*
        first slot is at 10x10
        slot size is 21x21
        1x1 padding between slots
    */

    bool Inventory::initialize(Bitmap_Font *font, Opengl_Texture *hud_sprite)
    {
        for (i32 slot_index = 0; slot_index < INVENTORY_SLOT_TOTAL_COUNT; slot_index++)
        {
            Inventory_Slot& slot = internal_data.slots[slot_index];
            slot.block_id = BlockId_Air;
            slot.count = 0;
        }

        internal_data.active_hot_bar_slot_index = 0;
        internal_data.dragging_slot_index = -1;
        internal_data.is_dragging = false;
        internal_data.dragging_slot_offset = { 0.0f, 0.0f };
        internal_data.font = font;
        internal_data.hud_sprite = hud_sprite;

        f32 hud_sprite_width  = (f32)hud_sprite->width;
        f32 hud_sprite_height = (f32)hud_sprite->height;

        internal_data.inventory_slot_uv_rect        = convert_texture_rect_to_uv_rect({ 0, 176, 32, 32 },   hud_sprite_width, hud_sprite_height);
        internal_data.active_inventory_slot_uv_rect = convert_texture_rect_to_uv_rect({ 0, 144, 32, 32 },   hud_sprite_width, hud_sprite_height);
        internal_data.inventory_hud_uv_rect         = convert_texture_rect_to_uv_rect({ 276, 0, 216, 194 }, hud_sprite_width, hud_sprite_height);

        internal_data.hot_bar_scale = 0.3f;

        return true;
    }

    void Inventory::shutdown(const std::string& world_path)
    {
        serialize(world_path);
    }

    void Inventory::add_block(u16 block_id)
    {
        bool found = false;

        for (i32 slot_index = 0; slot_index < INVENTORY_HOT_BAR_SLOT_COUNT + INVENTORY_SLOT_COUNT; slot_index++)
        {
            Inventory_Slot &slot = internal_data.slots[slot_index];

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
            for (i32 slot_index = 0; slot_index < INVENTORY_HOT_BAR_SLOT_COUNT + INVENTORY_SLOT_COUNT; slot_index++)
            {
                Inventory_Slot &slot = internal_data.slots[slot_index];
                bool is_slot_empty = slot.block_id == BlockId_Air && slot.count == 0;
                if (is_slot_empty)
                {
                    slot.block_id = block_id;
                    slot.count = 1;
                    found = true;
                    break;
                }
            }
        }

        if (!found)
        {
            // todo(harlequin): inventory is full drop the item
        }
    }

    void Inventory::calculate_slot_positions_and_sizes()
    {
        glm::vec2 frame_buffer_size = Opengl_Renderer::get_frame_buffer_size();
        f32 hot_bar_size_x = frame_buffer_size.x * internal_data.hot_bar_scale;
        auto& inventory_hud_pos = internal_data.inventory_hud_pos;
        auto& inventory_hud_size = internal_data.inventory_hud_size;
        auto& slot_size = internal_data.slot_size;
        auto& slot_padding = internal_data.slot_padding;
        auto& slot_positions = internal_data.slot_positions;
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
                Inventory_Slot& slot = internal_data.hot_bar[col];
                f32 slot_x = current_slot_x + (slot_size.x + slot_padding.x) * col;
                f32 slot_y = current_slot_y + (slot_size.y + slot_padding.y) * row;
                slot_positions[slot_index++] = make_rectangle2(slot_x, slot_y, slot_size.x, slot_size.y);
            }
        }
    }

    void Inventory::handle_input(Input *input)
    {
        const glm::vec2& mouse = input->mouse_position;

        for (i32 slot_index = 0; slot_index < INVENTORY_SLOT_TOTAL_COUNT; slot_index++)
        {
            Inventory_Slot &slot = internal_data.slots[slot_index];
            const Rectangle2 &slot_rect = internal_data.slot_positions[slot_index];
            bool is_slot_empty = slot.count == 0 && slot.block_id == BlockId_Air;

            if (!is_slot_empty &&
                is_point_inside_rectangle2(mouse, slot_rect) &&
                is_button_held(input, MC_MOUSE_BUTTON_LEFT) &&
                !internal_data.is_dragging)
            {
                internal_data.is_dragging = true;
                internal_data.dragging_slot_index = slot_index;
                internal_data.dragging_slot = slot;
                internal_data.dragging_slot_offset = mouse - slot_rect.min;
                break;
            }
        }

        auto& inventory_hud_pos = internal_data.inventory_hud_pos;
        auto& inventory_hud_size = internal_data.inventory_hud_size;
        auto& slot_size = internal_data.slot_size;
        auto& slot_padding = internal_data.slot_padding;
        auto& slot_positions = internal_data.slot_positions;
        auto half_slot_size = slot_size * 0.5f;

        if (is_button_released(input, MC_MOUSE_BUTTON_LEFT) && internal_data.is_dragging)
        {
            i32 closest_slot_index = -1;
            f32 best_distance_squared = Infinity32;

            for (i32 slot_index = 0; slot_index < INVENTORY_SLOT_TOTAL_COUNT; slot_index++)
            {
                Inventory_Slot &slot = internal_data.slots[slot_index];
                const Rectangle2 &slot_rect = internal_data.slot_positions[slot_index];

                glm::vec2 slot_center_p = slot_rect.min + half_slot_size;
                glm::vec2 mouse_to_slot = (mouse - internal_data.dragging_slot_offset + half_slot_size) - slot_center_p;

                f32 distance_to_slot_squared = glm::dot(mouse_to_slot, mouse_to_slot);
                if (distance_to_slot_squared < best_distance_squared)
                {
                    best_distance_squared = distance_to_slot_squared;
                    closest_slot_index    = slot_index;
                }
            }

            if (internal_data.dragging_slot_index != closest_slot_index)
            {
                Inventory_Slot& closest_slot = internal_data.slots[closest_slot_index];

                if (closest_slot.block_id != internal_data.dragging_slot.block_id)
                {
                    Inventory_Slot temp_slot = internal_data.slots[closest_slot_index];
                    internal_data.slots[closest_slot_index] = internal_data.dragging_slot;
                    internal_data.slots[internal_data.dragging_slot_index] = temp_slot;
                }
                else
                {
                    Inventory_Slot& dragging_slot = internal_data.slots[internal_data.dragging_slot_index];
                    i32 closest_slot_capacity = 64 - closest_slot.count;

                    if (closest_slot_capacity >= dragging_slot.count)
                    {
                        closest_slot.count += dragging_slot.count;
                        dragging_slot.block_id = BlockId_Air;
                        dragging_slot.count = 0;
                    }
                    else
                    {
                        closest_slot.count = 64;
                        dragging_slot.count -= closest_slot_capacity;
                    }
                }
            }

            internal_data.is_dragging = false;
            internal_data.dragging_slot_index = -1;
        }
    }

    static void draw_slot_at_index(World *world, i32 slot_index, glm::vec2 mouse)
    {
        auto& inventory_hud_pos = Inventory::internal_data.inventory_hud_pos;
        auto& inventory_hud_size = Inventory::internal_data.inventory_hud_size;
        auto& slot_size = Inventory::internal_data.slot_size;
        auto& slot_positions = Inventory::internal_data.slot_positions;
        auto half_slot_size = slot_size * 0.5f;

        Inventory_Slot &slot = Inventory::internal_data.slots[slot_index];
        Rectangle2 slot_rect = Inventory::internal_data.slot_positions[slot_index];

        glm::vec2 slot_pos = slot_rect.min;

        if (slot_index == Inventory::internal_data.dragging_slot_index)
        {
            slot_pos = mouse - Inventory::internal_data.dragging_slot_offset;
        }

        if (slot.block_id)
        {
            const Block_Info& info = world->block_infos[slot.block_id];
            UV_Rect& side_texture_uv_rect = texture_uv_rects[info.side_texture_id];
            glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

            // todo(harlequin): temparary
            if (info.flags & BlockFlags_Should_Color_Side_By_Biome)
            {
                f32 rgba_color_factor = 1.0f / 255.0f;
                glm::vec4 grass_color = { 109.0f, 184.0f, 79.0f, 255.0f };
                grass_color *= rgba_color_factor;
                color = grass_color;
            }

            Opengl_2D_Renderer::draw_rect(slot_pos + half_slot_size,
                                          slot_size,
                                          0.0f,
                                          color,
                                          &Opengl_Renderer::internal_data.block_sprite_sheet,
                                          side_texture_uv_rect.top_right - side_texture_uv_rect.bottom_left,
                                          side_texture_uv_rect.bottom_left);

            Bitmap_Font* font = Inventory::internal_data.font;

            std::stringstream ss;
            ss << (u32)slot.count;
            std::string text = ss.str();
            auto text_size = font->get_string_size(text);
            Opengl_2D_Renderer::draw_string(font, text, text_size, slot_pos + half_slot_size, { 1.0f, 1.0f, 1.0f, 1.0f });
        }
    }

    void Inventory::draw(World *world, Input *input)
    {
        const glm::vec2& mouse = input->mouse_position;

        auto& inventory_hud_pos = Inventory::internal_data.inventory_hud_pos;
        auto& inventory_hud_size = Inventory::internal_data.inventory_hud_size;

        Opengl_2D_Renderer::draw_rect(inventory_hud_pos,
                                      inventory_hud_size,
                                      0.0f,
                                      { 1.0f, 1.0f, 1.0f, 1.0f },
                                      internal_data.hud_sprite,
                                      internal_data.inventory_hud_uv_rect.top_right - internal_data.inventory_hud_uv_rect.bottom_left,
                                      internal_data.inventory_hud_uv_rect.bottom_left);

        for (i32 slot_index = 0; slot_index < INVENTORY_SLOT_TOTAL_COUNT; slot_index++)
        {
            if (slot_index != internal_data.dragging_slot_index)
            {
                draw_slot_at_index(world, slot_index, mouse);
            }
        }

        if (internal_data.dragging_slot_index != -1)
        {
            draw_slot_at_index(world, internal_data.dragging_slot_index, mouse);
        }
    }

    void Inventory::handle_hotbar_input(Input *input)
    {
        for (i32 key_code = MC_KEY_1, numpad_key_code = MC_KEY_KP_1; key_code <= MC_KEY_9; key_code++, numpad_key_code++)
        {
            if (is_key_pressed(input, key_code))
            {
                i32 slot_index = key_code - MC_KEY_1;
                internal_data.active_hot_bar_slot_index = slot_index;
            }
            else if (is_key_pressed(input, numpad_key_code))
            {
                i32 slot_index = numpad_key_code - MC_KEY_KP_1;
            internal_data.active_hot_bar_slot_index = slot_index;
            }
        }
    }

    void Inventory::draw_hotbar(World *world)
    {
        glm::vec2 frame_buffer_size = Opengl_Renderer::get_frame_buffer_size();

        f32 hot_bar_size_x = frame_buffer_size.x * internal_data.hot_bar_scale;

        f32 slot_width = hot_bar_size_x / INVENTORY_HOT_BAR_SLOT_COUNT;
        f32 slot_height = slot_width;
        f32 half_slot_width = slot_width * 0.5f;
        f32 half_slot_height = slot_height * 0.5f;

        f32 hot_bar_start_x = frame_buffer_size.x * 0.5f - hot_bar_size_x * 0.5f;        
        f32 hot_bar_offset_from_bottom = frame_buffer_size.y - half_slot_height;
        
        for (i32 slot_index = 0; slot_index < INVENTORY_HOT_BAR_SLOT_COUNT; slot_index++)
        {
            Inventory_Slot& slot = internal_data.hot_bar[slot_index];

            UV_Rectangle* slot_uv_rect = &internal_data.inventory_slot_uv_rect;

            if (internal_data.active_hot_bar_slot_index == slot_index)
            {
                slot_uv_rect = &internal_data.active_inventory_slot_uv_rect;
            }

            f32 slot_center_x = hot_bar_start_x + slot_index * slot_width + half_slot_width;
            f32 slot_center_y = hot_bar_offset_from_bottom - half_slot_height;

            if (slot.block_id)
            {
                const Block_Info& info = world->block_infos[slot.block_id];
                UV_Rect& side_texture_uv_rect = texture_uv_rects[info.side_texture_id];
                glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

                Opengl_2D_Renderer::draw_rect({ slot_center_x, slot_center_y },
                                              { slot_width, slot_height },
                                              0.0f,
                                              color,
                                              &Opengl_Renderer::internal_data.block_sprite_sheet,
                                              side_texture_uv_rect.top_right - side_texture_uv_rect.bottom_left,
                                              side_texture_uv_rect.bottom_left);

                std::stringstream ss;
                ss << (u32)slot.count;

                Bitmap_Font* font = internal_data.font;
                std::string text = ss.str();
                auto text_size = font->get_string_size(text);
                Opengl_2D_Renderer::draw_string(font, text, text_size, { slot_center_x, slot_center_y }, { 1.0f, 1.0f, 1.0f, 1.0f });
            }

            Opengl_2D_Renderer::draw_rect({ slot_center_x, slot_center_y },
                                          { slot_width, slot_height },
                                          0.0f,
                                          { 1.0f, 1.0f, 1.0f, 1.0f },
                                          internal_data.hud_sprite,
                                          slot_uv_rect->top_right - slot_uv_rect->bottom_left,
                                          slot_uv_rect->bottom_left);
        }
    }

    void Inventory::serialize(const std::string& world_path)
    {
        std::string inventory_filepath = world_path + "/inventory";
        FILE* file = fopen(inventory_filepath.c_str(), "wb");
        if (!file)
        {
            fprintf(stderr, "[ERROR]: failed to open file: %s for writing\n", inventory_filepath.c_str());
            return;
        }
        fwrite(internal_data.slots, sizeof(internal_data.slots), 1, file);
        fclose(file);
    }

    void Inventory::deserialize(const std::string& world_path)
    {
        std::string inventory_filepath = world_path + "/inventory";

        if (!std::filesystem::exists(inventory_filepath))
        {
            return;
        }

        FILE* file = fopen(inventory_filepath.c_str(), "rb");
        if (!file)
        {
            fprintf(stderr, "[ERROR]: failed to open file: %s for reading\n", inventory_filepath.c_str());
            return;
        }
        fread(internal_data.slots, sizeof(internal_data.slots), 1, file);
        fclose(file);
    }

    Inventory_Data Inventory::internal_data;
}
