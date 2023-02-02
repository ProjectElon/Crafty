#include "opengl_texture_atlas.h"
#include "memory/memory_arena.h"

namespace minecraft {

    bool initialize_texture_atlas(Opengl_Texture_Atlas *atlas,
                                  Opengl_Texture       *texture,
                                  u32                   sub_texture_count,
                                  Rectangle2i          *sub_texture_rectangles,
                                  Memory_Arena         *arena)
    {
        Assert(atlas);
        Assert(texture);
        Assert(sub_texture_count);
        Assert(sub_texture_rectangles);
        Assert(arena);

        atlas->texture                    = texture;
        atlas->sub_texture_count          = sub_texture_count;
        atlas->sub_texture_rectangles     = sub_texture_rectangles;
        atlas->sub_texture_texture_coords = ArenaPushArrayAligned(arena, Texture_Coords, sub_texture_count);

        for (u32 i = 0; i < sub_texture_count; i++)
        {
            // f32 one_over_width = 1.0f / texture_width;
            // f32 one_over_height = 1.0f / texture_height;

            // f32 new_y = texture_height - (f32)rect.y;
            // result.bottom_right = { ((f32)rect.x + rect.width) * one_over_width, (new_y - rect.height) * one_over_height };
            // result.bottom_left = { (f32)rect.x * one_over_width, (new_y - rect.height) * one_over_height };
            // result.top_left = { (f32)rect.x * one_over_width, (f32)new_y * one_over_height };
            // result.top_right = { ((f32)rect.x + rect.width) * one_over_width, (f32)new_y * one_over_height };

            // side_texture_uv_rect.top_right - side_texture_uv_rect.bottom_left,
            // side_texture_uv_rect.bottom_left

            Rectangle2i *rectangle = sub_texture_rectangles + i;

            f32 one_over_width  = 1.0f / (f32)texture->width;
            f32 one_over_height = 1.0f / (f32)texture->height;

            Texture_Coords *texture_coords = &atlas->sub_texture_texture_coords[i];
            texture_coords->scale  = { (f32)rectangle->width * one_over_width, (f32)rectangle->height                * one_over_height };
            texture_coords->offset = { (f32)rectangle->x     * one_over_width, (f32)(texture->height - rectangle->y - rectangle->height) * one_over_height };
        }

        return true;
    }

    const Texture_Coords* get_sub_texture_coords(Opengl_Texture_Atlas *atlas,
                                                 u32 sub_texture_index)
    {
        Assert(atlas);
        Assert(sub_texture_index < atlas->sub_texture_count);
        return &atlas->sub_texture_texture_coords[sub_texture_index];
    }
}
