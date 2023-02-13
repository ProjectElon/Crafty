#include "game/game_assets.h"
#include "core/file_system.h"
#include "memory/memory_arena.h"
#include "renderer/opengl_shader.h"
#include "renderer/opengl_texture.h"
#include "renderer/font.h"
#include "containers/hash_table.h"

// todo(harlequin): temprary
#include "meta/spritesheet_meta.h"

#include <filesystem>
#include "game_assets.h"

namespace minecraft {

    struct Game_Assets_State
    {
        Memory_Arena    asset_arena;
        Memory_Arena    asset_string_table_arena;
        Game_Asset_Info asset_infos[GameAssetType_Count];
        u32             asset_count;
        String8        *asset_string_table;
        Game_Asset     *assets;
    };

    static Game_Assets_State *state;

    static void set_asset_extensions(GameAssetType type,
                                     String8      *extensions,
                                     u32           count)
    {
        Game_Asset_Info *asset_info = &state->asset_infos[type];
        asset_info->extensions      = ArenaPushArrayAligned(&state->asset_arena,
                                                            String8,
                                                            count);
        asset_info->extension_count = count;
        memcpy(asset_info->extensions, extensions, count * sizeof(String8));
    }

    static GameAssetType find_asset_type(const String8 *extension)
    {
        GameAssetType asset_type = GameAssetType_None;

        for (u32 i = 1; i < GameAssetType_Count; i++)
        {
            Game_Asset_Info *asset_info = state->asset_infos + i;
            for (u32 j = 0; j < asset_info->extension_count; j++)
            {
                String8 *supported_extension = asset_info->extensions + j;
                if (equal(extension, supported_extension))
                {
                    asset_type = (GameAssetType)i;
                    break;
                }
            }
        }

        return asset_type;
    }

    bool initialize_game_assets(Memory_Arena *arena, const char *root_path)
    {
        if (state)
        {
            return false;
        }
        state = ArenaPushAlignedZero(arena, Game_Assets_State);
        Assert(state);

        state->asset_string_table_arena = push_sub_arena(arena, MegaBytes(1));
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

        state->asset_string_table = ArenaBeginArray(&state->asset_arena, String8);
        auto it = std::filesystem::recursive_directory_iterator(std::filesystem::path(root_path));

        for (const auto &entry : it)
        {
            if (entry.is_regular_file())
            {
                std::string asset_file_path = entry.path().string();
                for (size_t i = 0; i < asset_file_path.length(); i++)
                {
                    if (asset_file_path[i] == '\\')
                    {
                        asset_file_path[i] = '/';
                    }
                }

                String8 *str = ArenaPushArrayEntry(&state->asset_arena,
                                                    state->asset_string_table);

                *str = push_string8(&state->asset_string_table_arena,
                                    "%.*s",
                                    (i32)asset_file_path.length(),
                                    asset_file_path.c_str());
            }
        }

        state->asset_count = (u32)ArenaEndArray(&state->asset_arena,
                                                state->asset_string_table);

        state->assets = ArenaPushArrayAligned(&state->asset_arena,
                                              Game_Asset,
                                              state->asset_count);

        for (u32 i = 0; i < state->asset_count; i++)
        {
            Game_Asset *asset      = &state->assets[i];
            String8    *asset_path = &state->asset_string_table[i];
            i32 dot_index          = find_last_any_char(asset_path, ".");
            if (dot_index == -1)
            {
                fprintf(stderr,
                        "[ERROR]: asset path: %.*s missing extension",
                        (i32)asset_path->count,
                        asset_path->data);
            }
            String8 extension = sub_str(asset_path, dot_index + 1);
            asset->type       = find_asset_type(&extension);
        }

        return true;
    }

    void shutdown_game_assets()
    {
    }

    Asset_Handle find_asset(const String8 &path)
    {
        Asset_Handle handle = INVALID_ASSET_HANDLE;

        // todo(harlequin): should we use a hash table here ?
        for (u32 i = 0; i < state->asset_count; i++)
        {
            String8 *str = state->asset_string_table + i;
            if (equal(str, &path))
            {
                handle = i;
                break;
            }
        }

        return handle;
    }

    const Game_Asset *get_asset(Asset_Handle handle)
    {
        Assert(handle < state->asset_count);
        return &state->assets[handle];
    }

    // todo(harlequin): remove asset logging from load_texture load_shader load_font
    bool load_asset(Asset_Handle handle)
    {
        Assert(handle < state->asset_count);
        Game_Asset *asset = &state->assets[handle];
        if (asset->state == AssetState_Loaded)
        {
            return true;
        }

        String8 *asset_path = &state->asset_string_table[handle];
        asset->state = AssetState_Pending;

        switch (asset->type)
        {
            case GameAssetType_None:
            {
                asset->state = AssetState_Unloaded;
                fprintf(stderr,
                        "[ERROR]: failed to load asset: %.*s",
                        (i32)asset_path->count,
                             asset_path->data);
                return asset;
            } break;

            case GameAssetType_Texture:
            {
                Opengl_Texture *texture = ArenaPushAligned(&state->asset_arena,
                                                           Opengl_Texture);
                bool loaded = load_texture(texture,
                                           asset_path->data,
                                           TextureUsage_UI);
                if (!loaded)
                {
                    return false;
                }
                asset->data = texture;
            } break;

            case GameAssetType_Shader:
            {
                Opengl_Shader *shader = ArenaPushAligned(&state->asset_arena,
                                                         Opengl_Shader);
                bool loaded = load_shader(shader,
                                          asset_path->data);
                if (!loaded)
                {
                    return false;
                }
                asset->data = shader;
            } break;

            case GameAssetType_Font:
            {
                Bitmap_Font *font = ArenaPushAligned(&state->asset_arena,
                                                     Bitmap_Font);
                bool loaded = load_font(font,
                                        asset_path->data,
                                        22,
                                        &state->asset_arena);
                if (!loaded)
                {
                    return false;
                }

                asset->data = font;
            } break;
        }

        asset->state = AssetState_Loaded;
        return true;
    }

    Asset_Handle load_asset(const String8 &path)
    {
        Asset_Handle handle = find_asset(path);
        if (handle != INVALID_ASSET_HANDLE)
        {
            load_asset(handle);
        }
        return handle;
    }

    Opengl_Texture* get_texture(Asset_Handle handle)
    {
        Assert(handle < state->asset_count);
        Game_Asset *asset = &state->assets[handle];
        Assert(asset->type == GameAssetType_Texture);
        return (Opengl_Texture*)asset->data;
    }

    Opengl_Shader* get_shader(Asset_Handle handle)
    {
        Assert(handle < state->asset_count);
        Game_Asset *asset = &state->assets[handle];
        Assert(asset->type == GameAssetType_Shader);
        return (Opengl_Shader*)asset->data;
    }

    Bitmap_Font* get_font(Asset_Handle handle)
    {
        Assert(handle < state->asset_count);
        Game_Asset *asset = &state->assets[handle];
        Assert(asset->type == GameAssetType_Font);
        return (Bitmap_Font*)asset->data;
    }

    void load_game_assets(Game_Assets *assets)
    {
        assets->blocks_sprite_sheet = load_asset(String8FromCString("../assets/textures/block_spritesheet.png"));
        Opengl_Texture *block_sprite_sheet = get_texture(assets->blocks_sprite_sheet);
        set_texture_params_based_on_usage(block_sprite_sheet, TextureUsage_SpriteSheet);

        initialize_texture_atlas(&assets->blocks_atlas, block_sprite_sheet, MC_PACKED_TEXTURE_COUNT, (Rectangle2i*)texture_rects, &state->asset_arena);

        assets->hud_sprite = load_asset(String8FromCString("../assets/textures/hudSprites.png"));
        Opengl_Texture *hud_sprite_texture = get_texture(assets->hud_sprite);
        set_texture_params_based_on_usage(hud_sprite_texture, TextureUsage_UI);

        assets->gameplay_crosshair  = load_asset(String8FromCString("../assets/textures/crosshair/crosshair001.png"));
        Opengl_Texture *gameplay_crosshair_texture = get_texture(assets->gameplay_crosshair);
        set_texture_params_based_on_usage(gameplay_crosshair_texture, TextureUsage_UI);

        assets->inventory_crosshair = load_asset(String8FromCString("../assets/textures/crosshair/crosshair022.png"));
        Opengl_Texture *inventory_crosshair_texture = get_texture(assets->inventory_crosshair);
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