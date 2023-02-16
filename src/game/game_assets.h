#pragma once

#include "core/common.h"
#include "containers/string.h"

// todo(harlequin): temprary
#include "renderer/opengl_texture_atlas.h"

namespace minecraft {

    struct Memory_Arena;
    struct Opengl_Texture;
    struct Opengl_Shader;
    struct Opengl_Texture_Atlas;
    struct Bitmap_Font;

    enum GameAssetType : u32
    {
        GameAssetType_None,
        GameAssetType_Texture,
        GameAssetType_Shader,
        GameAssetType_Font,
        GameAssetType_TextureAtlas,
        GameAssetType_Count
    };

    struct Game_Asset_Info
    {
        u32      extension_count;
        String8 *extensions;
    };

    typedef u32 Asset_Handle;
    #define INVALID_ASSET_HANDLE (~0u)

    enum AssetState : u8
    {
        AssetState_Unloaded = 0x0,
        AssetState_Pending  = 0x1,
        AssetState_Loaded   = 0x2
    };

    struct Game_Asset_Entry
    {
        String8      *path;
        GameAssetType type;
        AssetState    state;
        u64           size;
        void         *data;
    };

    bool initialize_game_assets(Memory_Arena *arena,
                                const char   *root_path);
    void shutdown_game_assets();

    bool is_asset_handle_valid(Asset_Handle handle);

    Asset_Handle find_asset(const String8 &path);

    Asset_Handle load_asset(const String8 &path);
    bool load_asset(Asset_Handle asset_handle);

    const Game_Asset_Entry* get_asset(Asset_Handle asset_handle);

    Opengl_Texture* get_texture(Asset_Handle handle);
    Opengl_Shader* get_shader(Asset_Handle handle);
    Opengl_Texture_Atlas *get_texture_atlas(Asset_Handle handle);
    Bitmap_Font* get_font(Asset_Handle handle);

    struct Game_Assets
    {
        Asset_Handle blocks_sprite_sheet;
        Asset_Handle hud_sprite;
        Asset_Handle gameplay_crosshair;
        Asset_Handle inventory_crosshair;

        Asset_Handle blocks_atlas;

        Asset_Handle basic_shader;
        Asset_Handle block_shader;
        Asset_Handle composite_shader;
        Asset_Handle line_shader;
        Asset_Handle opaque_chunk_shader;
        Asset_Handle transparent_chunk_shader;
        Asset_Handle screen_shader;
        Asset_Handle quad_shader;

        Asset_Handle fira_code_font;
        Asset_Handle noto_mono_font;
        Asset_Handle consolas_mono_font;
        Asset_Handle liberation_mono_font;
    };

    void load_game_assets(Game_Assets *assets);
}