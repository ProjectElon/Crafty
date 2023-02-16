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
        Memory_Arena asset_entry_arena;
        Memory_Arena asset_string_arena;
        Memory_Arena asset_path_arena;
        Memory_Arena asset_storage_arena;

        Game_Asset_Info asset_infos[GameAssetType_Count];

        u32               asset_count;
        String8          *asset_paths;
        Game_Asset_Entry *asset_entries;
    };

    static Game_Assets_State *game_assets_state;

    static void set_asset_extensions(GameAssetType type,
                                     String8      *extensions,
                                     u32           count)
    {
        Game_Asset_Info *asset_info = &game_assets_state->asset_infos[type];
        asset_info->extensions      = ArenaPushArrayAligned(&game_assets_state->asset_entry_arena,
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
            Game_Asset_Info *asset_info = game_assets_state->asset_infos + i;
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

    // todo(harlequin): remove asset logging from load_texture load_shader load_font
    static bool load_asset(Game_Asset_Entry *asset_entry, const String8 &path)
    {
        Assert(asset_entry);
        Assert(asset_entry->state == AssetState_Unloaded);

        asset_entry->state = AssetState_Pending;

        switch (asset_entry->type)
        {
            case GameAssetType_Texture:
            {
                Opengl_Texture *texture = ArenaPushAligned(&game_assets_state->asset_storage_arena,
                                                           Opengl_Texture);
                bool loaded = load_texture(texture,
                                           path.data,
                                           TextureUsage_UI);
                if (!loaded)
                {
                    asset_entry->state = AssetState_Unloaded;
                    return false;
                }
                asset_entry->data = texture;
            } break;

            case GameAssetType_Shader:
            {
                Opengl_Shader *shader = ArenaPushAligned(&game_assets_state->asset_storage_arena,
                                                         Opengl_Shader);
                bool loaded = load_shader(shader,
                                          path.data);
                if (!loaded)
                {
                    asset_entry->state = AssetState_Unloaded;
                    return false;
                }
                asset_entry->data = shader;
            } break;

            case GameAssetType_Font:
            {
                Bitmap_Font *font = ArenaPushAligned(&game_assets_state->asset_storage_arena,
                                                     Bitmap_Font);
                bool loaded = load_font(font,
                                        path.data,
                                        18,
                                        &game_assets_state->asset_storage_arena);
                if (!loaded)
                {
                    asset_entry->state = AssetState_Unloaded;
                    return false;
                }

                asset_entry->data = font;
            } break;

            case GameAssetType_TextureAtlas:
            {
                Opengl_Texture_Atlas *atlas = ArenaPushAligned(&game_assets_state->asset_storage_arena,
                                                               Opengl_Texture_Atlas);
                bool loaded = deserialize_texture_atlas(atlas,
                                                        path.data,
                                                        &game_assets_state->asset_storage_arena);
                if (!loaded)
                {
                    asset_entry->state = AssetState_Unloaded;
                    return false;
                }

                asset_entry->data = atlas;
            } break;

            default:
            {
                asset_entry->state = AssetState_Unloaded;
                return false;
            } break;
        }

        asset_entry->state = AssetState_Loaded;
        return true;
    }

    bool initialize_game_assets(Memory_Arena *arena, const char *root_path)
    {
        if (game_assets_state)
        {
            return false;
        }
        game_assets_state = ArenaPushAlignedZero(arena, Game_Assets_State);
        Assert(game_assets_state);


        game_assets_state->asset_string_arena  = push_sub_arena(arena, MegaBytes(1));
        game_assets_state->asset_path_arena    = push_sub_arena(arena, MegaBytes(1));
        game_assets_state->asset_entry_arena   = push_sub_arena(arena, MegaBytes(1));
        game_assets_state->asset_storage_arena = push_sub_arena(arena, MegaBytes(64));

        String8 texture_extensions[] =
        {
            String8FromCString("png")
        };
        set_asset_extensions(GameAssetType_Texture, texture_extensions, ArrayCount(texture_extensions));

        String8 shader_extensions[] =
        {
            String8FromCString("glsl")
        };
        set_asset_extensions(GameAssetType_Shader, shader_extensions, ArrayCount(shader_extensions));

        String8 font_extensions[] =
        {
            String8FromCString("ttf")
        };
        set_asset_extensions(GameAssetType_Font, font_extensions, ArrayCount(font_extensions));

        String8 texture_atlas_extensions[] =
        {
            String8FromCString("atlas")
        };
        set_asset_extensions(GameAssetType_TextureAtlas,
                             texture_atlas_extensions,
                             ArrayCount(texture_atlas_extensions));

        game_assets_state->asset_paths   = ArenaBeginArray(&game_assets_state->asset_path_arena, String8);
        game_assets_state->asset_entries = ArenaBeginArray(&game_assets_state->asset_entry_arena, Game_Asset_Entry);

        namespace fs = std::filesystem;
        auto it = fs::recursive_directory_iterator(fs::path(root_path));

        for (const auto &entry : it)
        {
            if (entry.is_regular_file())
            {
                const fs::path &file_path = entry.path();
                std::string asset_file_path = file_path.string();

                for (size_t i = 0; i < asset_file_path.length(); i++)
                {
                    if (asset_file_path[i] == '\\')
                    {
                        asset_file_path[i] = '/';
                    }
                }

                String8 *asset_path = ArenaPushArrayEntry(&game_assets_state->asset_path_arena,
                                                           game_assets_state->asset_paths);

                Game_Asset_Entry *asset_entry = ArenaPushArrayEntry(&game_assets_state->asset_entry_arena,
                                                                    game_assets_state->asset_entries);

                *asset_path = push_string8(&game_assets_state->asset_string_arena,
                                           "%.*s",
                                           (i32)asset_file_path.length(),
                                           asset_file_path.c_str());

                asset_entry->path = asset_path;
                asset_entry->size = fs::file_size(file_path);

                i32 dot_index = find_last_any_char(asset_path, ".");
                if (dot_index == -1)
                {
                    fprintf(stderr,
                            "[ERROR]: asset path: %.*s missing extension",
                            (i32)asset_path->count,
                            asset_path->data);
                }
                String8 extension = sub_str(asset_path, dot_index + 1);
                asset_entry->type = find_asset_type(&extension);
                if (asset_entry->type == GameAssetType_None)
                {
                    fprintf(stderr,
                            "[ERROR]: failed to load unregistered asset: %.*s\n",
                            (i32)asset_path->count,
                            asset_path->data);
                }
            }
        }

        game_assets_state->asset_count = (u32)ArenaEndArray(&game_assets_state->asset_entry_arena,
                                                             game_assets_state->asset_paths);

        return true;
    }

    void shutdown_game_assets()
    {
    }

    bool is_asset_handle_valid(Asset_Handle handle)
    {
        return handle < game_assets_state->asset_count;
    }

    Asset_Handle find_asset(const String8 &path)
    {
        Asset_Handle handle = INVALID_ASSET_HANDLE;

        // todo(harlequin): should we use a hash table here ?
        for (u32 i = 0; i < game_assets_state->asset_count; i++)
        {
            String8 *str = game_assets_state->asset_paths + i;
            if (equal(str, &path))
            {
                handle = i;
                break;
            }
        }

        return handle;
    }

    const Game_Asset_Entry *get_asset(Asset_Handle asset_handle)
    {
        Assert(is_asset_handle_valid(asset_handle));
        return &game_assets_state->asset_entries[asset_handle];
    }

    Asset_Handle load_asset(const String8 &path)
    {
        Asset_Handle asset_handle = find_asset(path);
        if (asset_handle != INVALID_ASSET_HANDLE)
        {
            Game_Asset_Entry *asset_entry = &game_assets_state->asset_entries[asset_handle];
            if (asset_entry->state == AssetState_Unloaded)
            {
                bool loaded = load_asset(asset_entry, path);
            }
        }
        return asset_handle;
    }

    bool load_asset(Asset_Handle asset_handle)
    {
        Assert(is_asset_handle_valid(asset_handle));
        Game_Asset_Entry *asset_entry = &game_assets_state->asset_entries[asset_handle];
        if (asset_entry->state == AssetState_Unloaded)
        {
            load_asset(asset_entry, *asset_entry->path);
        }
        return asset_entry->state == AssetState_Loaded;
    }

    Opengl_Texture* get_texture(Asset_Handle handle)
    {
        Assert(handle < game_assets_state->asset_count);
        Game_Asset_Entry *asset = &game_assets_state->asset_entries[handle];
        Assert(asset->type == GameAssetType_Texture);
        return (Opengl_Texture*)asset->data;
    }

    Opengl_Shader* get_shader(Asset_Handle handle)
    {
        Assert(handle < game_assets_state->asset_count);
        Game_Asset_Entry *asset = &game_assets_state->asset_entries[handle];
        Assert(asset->type == GameAssetType_Shader);
        return (Opengl_Shader*)asset->data;
    }

    Opengl_Texture_Atlas *get_texture_atlas(Asset_Handle handle)
    {
        Assert(handle < game_assets_state->asset_count);
        Game_Asset_Entry *asset = &game_assets_state->asset_entries[handle];
        Assert(asset->type == GameAssetType_TextureAtlas);
        return (Opengl_Texture_Atlas*)asset->data;
    }

    Bitmap_Font* get_font(Asset_Handle handle)
    {
        Assert(handle < game_assets_state->asset_count);
        Game_Asset_Entry *asset = &game_assets_state->asset_entries[handle];
        Assert(asset->type == GameAssetType_Font);
        return (Bitmap_Font*)asset->data;
    }

    void load_game_assets(Game_Assets *assets)
    {
        assets->blocks_atlas = load_asset(String8FromCString("../assets/textures/blocks.atlas"));

        assets->blocks_sprite_sheet = load_asset(String8FromCString("../assets/textures/block_spritesheet.png"));
        Opengl_Texture *block_sprite_sheet = get_texture(assets->blocks_sprite_sheet);
        set_texture_params_based_on_usage(block_sprite_sheet, TextureUsage_SpriteSheet);

#if 0
        String8 *names = ArenaPushArray(&game_assets_state->asset_storage_arena, String8, MC_PACKED_TEXTURE_COUNT);

        for (u32 i = 0; i < MC_PACKED_TEXTURE_COUNT; i++)
        {
            names[i] = { (char*)texture_names[i], strlen(texture_names[i]) };
        }

        initialize_texture_atlas(&assets->blocks_atlas,
                                 assets->blocks_sprite_sheet,
                                 MC_PACKED_TEXTURE_COUNT,
                                 (Rectanglei*)texture_rects,
                                 names,
                                 &game_assets_state->asset_storage_arena);

        bool success = serialize_texture_atlas(&assets->blocks_atlas,
                                               "../assets/textures/blocks.atlas");
        Assert(success);
        success = deserialize_texture_atlas(&assets->blocks_atlas,
                                            "../assets/textures/blocks.atlas",
                                            &game_assets_state->asset_storage_arena);
        Assert(success);
        Assert(get_sub_texture_index(&assets->blocks_atlas, String8FromCString("water")) == Texture_Id_water);
#endif
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