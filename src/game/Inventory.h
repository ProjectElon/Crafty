#pragma once

#include "core/common.h"
#include "containers/string.h"
#include "game/math.h"
#include <glm/glm.hpp>

#define INVENTORY_INVALID_SLOT_INDEX -1
#define INVENTORY_HOT_BAR_SLOT_COUNT 9
#define INVENTORY_ROW_COUNT 3
#define INVENTORY_COLOUM_COUNT 9
#define INVENTORY_SLOT_COUNT INVENTORY_ROW_COUNT * INVENTORY_COLOUM_COUNT
#define INVENTORY_CRAFTING_SLOTS_COUNT 10
#define INVENTORY_SLOT_TOTAL_COUNT INVENTORY_HOT_BAR_SLOT_COUNT + INVENTORY_SLOT_COUNT + INVENTORY_CRAFTING_SLOTS_COUNT

namespace minecraft {

    struct Bitmap_Font;
    struct Opengl_Texture;
    struct Opengl_Texture_Atlas;
    struct Input;
    struct World;
    struct Temprary_Memory_Arena;
    struct Game_Assets;

    struct Inventory_Slot
    {
        u16 block_id;
        u8  count;
    };

    struct Inventory
    {
        union
        {
            struct
            {
                Inventory_Slot hot_bar[INVENTORY_HOT_BAR_SLOT_COUNT];
                Inventory_Slot main[INVENTORY_SLOT_COUNT];
                Inventory_Slot crafting[INVENTORY_CRAFTING_SLOTS_COUNT];
            };

            Inventory_Slot slots[INVENTORY_SLOT_TOTAL_COUNT];
        };

        u32 active_hot_bar_slot_index;

        bool            is_dragging;
        Inventory_Slot  dragging_slot;
        i32             dragging_slot_index;
        glm::vec2       dragging_slot_offset;

        Bitmap_Font          *font;
#if 0
        Opengl_Texture       *blocks_sprite_sheet;
#else
        Opengl_Texture_Atlas *blocks_atlas;
#endif
        Opengl_Texture       *hud_sprite;

        // todo(harlequin): inventory.atlas
        Texture_Coords inventory_slot_texture_coords;
        Texture_Coords active_inventory_slot_texture_coords;
        Texture_Coords inventory_hud_texture_coords;

        f32             hot_bar_scale;
        f32             hot_bar_size;

        glm::vec2       inventory_hud_pos;
        glm::vec2       inventory_hud_size;
        glm::vec2       slot_size;
        glm::vec2       slot_padding;

        Rectangle slot_positions[INVENTORY_SLOT_TOTAL_COUNT];
    };

    bool initialize_inventory(Inventory          *inventory,
                              Game_Assets *assets);

    void shutdown_inventory(Inventory *inventory, String8 path, Temprary_Memory_Arena *temp_arena);

    bool add_block_to_inventory(Inventory *inventory, u16 block_id);

    void calculate_slot_positions_and_sizes(Inventory *inventory, const glm::vec2& frame_buffer_size);
    void handle_inventory_input(Inventory *inventory, Input *input);
    void draw_inventory(Inventory *inventory,
                        World *world,
                        Input *input,
                        Temprary_Memory_Arena *temp_arena);

    void handle_hotbar_input(Inventory *inventory, Input *input);
    void draw_hotbar(Inventory *inventory,
                     World *world,
                     const glm::vec2& frame_buffer_size,
                     Temprary_Memory_Arena *temp_arena);

    void serialize_inventory(Inventory *inventory, String8 path, Temprary_Memory_Arena *temp_arena);
    void deserialize_inventory(Inventory *inventory, String8 path, Temprary_Memory_Arena *temp_arena);
}
