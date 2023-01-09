#include "game/game_assets.h"
#include "memory/memory_arena.h"
#include "renderer/opengl_shader.h"
#include "renderer/opengl_texture.h"
#include "renderer/font.h"

namespace minecraft {

    struct Game_Assets_State
    {
        Memory_Arena    asset_arena;
        Game_Asset_Info asset_infos[GameAssetType_Count];
    };

    static Game_Assets_State *state;

    void set_asset_extensions(GameAssetType type, String8 *extensions, u32 count)
    {
        Game_Asset_Info *asset_info = &state->asset_infos[type];
        asset_info->extensions      = ArenaPushArrayAligned(&state->asset_arena,
                                                            String8,
                                                            count);
        asset_info->extension_count = count;
        memcpy(asset_info->extensions, extensions, count * sizeof(String8));
    }

    bool initialize_game_assets(Memory_Arena *arena)
    {
        if (state)
        {
            return false;
        }
        state = ArenaPushAlignedZero(arena, Game_Assets_State);
        Assert(state);

        u64 asset_arena_size   = MegaBytes(1);
        u8 *asset_arena_memory = ArenaPushArrayAlignedZero(arena, u8, asset_arena_size);
        state->asset_arena     = create_memory_arena(asset_arena_memory, asset_arena_size);

        // todo(harlequin): need to copy the array
        String8 texture_extensions[] = {
            Str8("png")
        };
        set_asset_extensions(GameAssetType_Texture, texture_extensions, ArrayCount(texture_extensions));

        String8 shader_extensions[] = {
            Str8("glsl")
        };
        set_asset_extensions(GameAssetType_Shader, shader_extensions, ArrayCount(shader_extensions));

        String8 font_extensions[] = {
            Str8("ttf")
        };
        set_asset_extensions(GameAssetType_Font, font_extensions, ArrayCount(font_extensions));

        return true;
    }

    void shutdown_game_assets()
    {
    }

    Game_Asset load_asset(String8 path)
    {
        Game_Asset asset = {};
        asset.path       = path;

        i32 dot_index    = find_last_any_char(&path, ".");
        if (dot_index == -1)
        {
            fprintf(stderr,
                    "[ERROR]: asset path: %.*s missing extension",
                    (i32)path.count,
                    path.data);
            return asset;
        }
        // todo(harlequin): String8 substring concept
        String8 asset_extension  = { path.data + dot_index + 1, path.count - dot_index - 1 };
        GameAssetType asset_type = GameAssetType_None;

        for (u32 i = 1; i < GameAssetType_Count; i++)
        {
            Game_Asset_Info *asset_info = state->asset_infos + i;
            for (u32 j = 0; j < asset_info->extension_count; j++)
            {
                String8 *supported_extension = asset_info->extensions + j;
                if (equal(&asset_extension, supported_extension))
                {
                    asset_type = (GameAssetType)i;
                    break;
                }
            }
        }

        asset.type = asset_type;

        switch (asset_type)
        {
            case GameAssetType_None:
            {
                fprintf(stderr,
                        "[ERROR]: unsupported asset extension: %.*s",
                        (i32)asset_extension.count,
                        asset_extension.data);
                return asset;
            } break;

            case GameAssetType_Texture:
            {
                Opengl_Texture *texture = ArenaPushAligned(&state->asset_arena,
                                                           Opengl_Texture);
                load_texture(texture, path.data, TextureUsage_UI);
                asset.data = texture;
            } break;

            case GameAssetType_Shader:
            {
                Opengl_Shader *shader = ArenaPushAligned(&state->asset_arena,
                                                         Opengl_Shader);
                load_shader(shader, path.data);
                asset.data = shader;
            } break;

            case GameAssetType_Font:
            {
                Bitmap_Font *font = ArenaPushAligned(&state->asset_arena,
                                                     Bitmap_Font);
                load_font(font, path.data, 22);
                asset.data = font;
            } break;
        }

        return asset;
    }
}