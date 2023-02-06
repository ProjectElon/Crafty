#include "game/game_assets.h"
#include "memory/memory_arena.h"
#include "renderer/opengl_shader.h"
#include "renderer/opengl_texture.h"
#include "renderer/font.h"

// todo(harlequin): temprary
#include "meta/spritesheet_meta.h"

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

        state->asset_arena = push_sub_arena(arena, MegaBytes(64));

        String8 texture_extensions[] = {
            String8FromCString("png")
        };
        set_asset_extensions(GameAssetType_Texture, texture_extensions, ArrayCount(texture_extensions));

        String8 shader_extensions[] = {
            String8FromCString("glsl")
        };
        set_asset_extensions(GameAssetType_Shader, shader_extensions, ArrayCount(shader_extensions));

        String8 font_extensions[] = {
            String8FromCString("ttf")
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

        String8 asset_extension  = sub_str(&path, dot_index + 1);
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
                load_font(font, path.data, 20, &state->asset_arena);
                asset.data = font;
            } break;
        }

        return asset;
    }

    Opengl_Texture* get_texture(Game_Asset *asset)
    {
        Assert(asset->type == GameAssetType_Texture);
        return (Opengl_Texture*)asset->data;
    }

    Opengl_Shader* get_shader(Game_Asset *asset)
    {
        Assert(asset->type == GameAssetType_Shader);
        return (Opengl_Shader*)asset->data;
    }

    Bitmap_Font* get_font(Game_Asset *asset)
    {
        Assert(asset->type == GameAssetType_Font);
        return (Bitmap_Font*)asset->data;
    }

    void load_game_assets(Game_Assets *assets)
    {
        assets->blocks_sprite_sheet = load_asset(String8FromCString("../assets/textures/block_spritesheet.png"));
        Opengl_Texture *block_sprite_sheet = get_texture(&assets->blocks_sprite_sheet);
        set_texture_params_based_on_usage(block_sprite_sheet, TextureUsage_SpriteSheet);

        initialize_texture_atlas(&assets->blocks_atlas, block_sprite_sheet, MC_PACKED_TEXTURE_COUNT, (Rectangle2i*)texture_rects, &state->asset_arena);

        assets->hud_sprite = load_asset(String8FromCString("../assets/textures/hudSprites.png"));
        Opengl_Texture *hud_sprite_texture = get_texture(&assets->hud_sprite);
        set_texture_params_based_on_usage(hud_sprite_texture, TextureUsage_UI);

        assets->gameplay_crosshair  = load_asset(String8FromCString("../assets/textures/crosshair/crosshair001.png"));
        Opengl_Texture *gameplay_crosshair_texture = get_texture(&assets->gameplay_crosshair);
        set_texture_params_based_on_usage(gameplay_crosshair_texture, TextureUsage_UI);

        assets->inventory_crosshair = load_asset(String8FromCString("../assets/textures/crosshair/crosshair022.png"));
        Opengl_Texture *inventory_crosshair_texture = get_texture(&assets->inventory_crosshair);
        set_texture_params_based_on_usage(inventory_crosshair_texture, TextureUsage_UI);

        assets->basic_shader = load_asset(String8FromCString("../assets/shaders/basic.glsl"));
        assets->block_shader = load_asset(String8FromCString("../assets/shaders/block.glsl"));
        assets->composite_shader = load_asset(String8FromCString("../assets/shaders/composite.glsl"));
        assets->line_shader = load_asset(String8FromCString("../assets/shaders/line.glsl"));
        assets->opaque_chunk_shader = load_asset(String8FromCString("../assets/shaders/opaque_chunk.glsl"));
        assets->transparent_chunk_shader = load_asset(String8FromCString("../assets/shaders/transparent_chunk.glsl"));
        assets->screen_shader = load_asset(String8FromCString("../assets/shaders/screen.glsl"));
        assets->quad_shader = load_asset(String8FromCString("../assets/shaders/quad.glsl"));

        assets->fira_code_font       = load_asset(String8FromCString("../assets/fonts/FiraCode-Regular.ttf"));
        assets->noto_mono_font       = load_asset(String8FromCString("../assets/fonts/NotoMono-Regular.ttf"));
        assets->consolas_mono_font   = load_asset(String8FromCString("../assets/fonts/Consolas.ttf"));
        assets->liberation_mono_font = load_asset(String8FromCString("../assets/fonts/liberation-mono.ttf"));
    }
}