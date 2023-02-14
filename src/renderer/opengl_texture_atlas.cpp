#include "opengl_texture_atlas.h"
#include "memory/memory_arena.h"
#include "game/game_assets.h"

namespace minecraft {

    bool initialize_texture_atlas(Opengl_Texture_Atlas *atlas,
                                  u32                   texture_asset_handle,
                                  u32                   sub_texture_count,
                                  Rectangle2i          *sub_texture_rectangles,
                                  Memory_Arena         *arena)
    {
        Assert(atlas);
        Assert(is_asset_handle_valid(texture_asset_handle));
        Assert(sub_texture_count);
        Assert(sub_texture_rectangles);
        Assert(arena);

        Opengl_Texture *texture = get_texture(texture_asset_handle);

        atlas->texture                    = texture;
        atlas->sub_texture_count          = sub_texture_count;
        atlas->sub_texture_rectangles     = sub_texture_rectangles;
        atlas->sub_texture_texture_coords = ArenaPushArrayAligned(arena,
                                                                  Texture_Coords,
                                                                  sub_texture_count);

        f32 one_over_width  = 1.0f / (f32)texture->width;
        f32 one_over_height = 1.0f / (f32)texture->height;

        for (u32 i = 0; i < sub_texture_count; i++)
        {
            // todo(harlequin): validate rectangles
            Rectangle2i *rectangle  = sub_texture_rectangles + i;
            Texture_Coords *texture_coords = &atlas->sub_texture_texture_coords[i];
            texture_coords->scale  = { (f32)rectangle->width  * one_over_width,
                                       (f32)rectangle->height * one_over_height };
            texture_coords->offset = { (f32)rectangle->x      * one_over_width,
                                       (f32)(texture->height - rectangle->y - rectangle->height) * one_over_height };
        }

        return true;
    }

    const Texture_Coords* get_sub_texture_coords(Opengl_Texture_Atlas *atlas,
                                                 u32                  sub_texture_index)
    {
        Assert(atlas);
        Assert(sub_texture_index < atlas->sub_texture_count);
        return &atlas->sub_texture_texture_coords[sub_texture_index];
    }

    bool serialize_texture_atlas(Opengl_Texture_Atlas *atlas,
                                 const char           *file_path)
    {
        assert(atlas);
        assert(is_asset_handle_valid(atlas->texture_asset_handle));

        FILE *file_handle = fopen(file_path, "wb");
        if (!file_handle)
        {
            fprintf(stderr,
                    "failed to open file %s for writing\n",
                    file_path);
            return false;
        }

        defer
        {
            fclose(file_handle);
        };

        const Game_Asset_Entry *texture_asset = get_asset(atlas->texture_asset_handle);
        return true;
    }

    bool deserialize_texture_atlas(Opengl_Texture_Atlas *atlas, const char *file_path)
    {
        return false;
    }
}
