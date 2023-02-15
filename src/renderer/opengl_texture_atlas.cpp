#include "opengl_texture_atlas.h"
#include "memory/memory_arena.h"
#include "containers/string.h"
#include "game/game_assets.h"

namespace minecraft {

    bool initialize_texture_atlas(Opengl_Texture_Atlas *atlas,
                                  u32                   texture_asset_handle,
                                  u32                   sub_texture_count,
                                  Rectangle2i          *sub_texture_rectangles,
                                  String8              *sub_texture_names,
                                  Memory_Arena         *arena)
    {
        Assert(atlas);
        Assert(is_asset_handle_valid(texture_asset_handle));
        Assert(sub_texture_count);
        Assert(sub_texture_rectangles);
        Assert(arena);

        Opengl_Texture *texture = get_texture(texture_asset_handle);

        atlas->texture_asset_handle   = texture_asset_handle;
        atlas->texture                = texture;
        atlas->sub_texture_count      = sub_texture_count;
        atlas->sub_texture_rectangles = sub_texture_rectangles;
        atlas->sub_texture_names      = sub_texture_names;

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

    u32 get_sub_texture_index(Opengl_Texture_Atlas *atlas,
                              const String8        &sub_texture_name)
    {
        for (u32 i = 0; i < atlas->sub_texture_count; i++)
        {
            if (equal(&sub_texture_name, &atlas->sub_texture_names[i]))
            {
                return i;
            }
        }

        return INVALID_SUB_TEXTURE_INDEX;
    }

    // todo(harlequin): should we buffer our fwrite calls into an arena or not ?
    bool serialize_texture_atlas(Opengl_Texture_Atlas  *atlas,
                                 const char            *file_path)
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
        String8 *asset_file_path = texture_asset->path;

        fwrite(&asset_file_path->count, sizeof(u64), 1, file_handle);
        fwrite(asset_file_path->data, sizeof(char) * asset_file_path->count, 1, file_handle);
        fwrite(&atlas->sub_texture_count, sizeof(u32), 1, file_handle);
        fwrite(atlas->sub_texture_rectangles,
               sizeof(Rectangle2i) * atlas->sub_texture_count,
               1,
               file_handle);

        for (u32 i = 0; i < atlas->sub_texture_count; i++)
        {
            String8 *sub_texture_name = &atlas->sub_texture_names[i];
            fwrite(&sub_texture_name->count,
                   sizeof(u64),
                   1,
                   file_handle);
            fwrite(sub_texture_name->data,
                   sizeof(char) * sub_texture_name->count,
                   1,
                   file_handle);
        }

        return true;
    }

    bool deserialize_texture_atlas(Opengl_Texture_Atlas *atlas,
                                   const char           *file_path,
                                   Memory_Arena         *arena)
    {

        assert(atlas);
        FILE *file_handle = fopen(file_path, "rb");

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

        Temprary_Memory_Arena temp_arena = begin_temprary_memory_arena(arena);

        String8 asset_file_path = String8FromCString("");
        fread(&asset_file_path.count, sizeof(u64), 1, file_handle);
        if (asset_file_path.count)
        {
            asset_file_path.data = ArenaPushArray(&temp_arena,
                                                  char,
                                                  asset_file_path.count + 1);
            asset_file_path.data[asset_file_path.count] = '\0';
            fread(asset_file_path.data,
                  sizeof(char) * asset_file_path.count,
                  1,
                  file_handle);
        }

        Asset_Handle asset_handle = find_asset(asset_file_path);
        Assert(is_asset_handle_valid(asset_handle));
        end_temprary_memory_arena(&temp_arena);

        u32 sub_texture_count;
        fread(&sub_texture_count, sizeof(u32), 1, file_handle);
        Assert(sub_texture_count);

        Rectangle2i *sub_texture_rectangles = ArenaPushArrayAligned(arena,
                                                                    Rectangle2i,
                                                                    sub_texture_count);
        fread(sub_texture_rectangles,
              sizeof(Rectangle2i) * sub_texture_count,
              1,
              file_handle);

        String8 *sub_texture_names = ArenaPushArray(arena,
                                                    String8,
                                                    sub_texture_count);

        for (u32 i = 0; i < atlas->sub_texture_count; i++)
        {
            String8 *sub_texture_name = &sub_texture_names[i];

            fread(&sub_texture_name->count,
                   sizeof(u64),
                   1,
                   file_handle);

            sub_texture_name->data = ArenaPushArray(arena, char, sub_texture_name->count + 1);
            sub_texture_name->data[sub_texture_name->count] = '\0';

            fread(sub_texture_name->data,
                  sizeof(char) * sub_texture_name->count,
                  1,
                  file_handle);
        }

        bool success = initialize_texture_atlas(atlas,
                                                asset_handle,
                                                sub_texture_count,
                                                sub_texture_rectangles,
                                                sub_texture_names,
                                                arena);
        return success;
    }
}
