#pragma once

#include "core/common.h"
#include "containers/string.h"

namespace minecraft {

    struct Memory_Arena;
    struct Opengl_Texture;
    struct Opengl_Shader;
    struct Bitmap_Font;

    enum GameAssetType : u32
    {
        GameAssetType_None,
        GameAssetType_Texture,
        GameAssetType_Shader,
        GameAssetType_Font,
        GameAssetType_Count
    };

    struct Game_Asset_Info
    {
        u32       extension_count;
        String8  *extensions;
    };

    struct Game_Asset
    {
        GameAssetType type;
        String8       path;
        void         *data;
    };

    bool initialize_game_assets(Memory_Arena *arena);
    void shutdown_game_assets();

    Game_Asset load_asset(String8 path);

    Opengl_Texture* get_texture(Game_Asset *asset);
    Opengl_Shader* get_shader(Game_Asset *asset);
    Bitmap_Font* get_font(Game_Asset *asset);

    struct Game_Assets
    {
        Game_Asset blocks_sprite_sheet;
        Game_Asset hud_sprite;
        Game_Asset gameplay_crosshair;
        Game_Asset inventory_crosshair;

        Game_Asset basic_shader;
        Game_Asset block_shader;
        Game_Asset composite_shader;
        Game_Asset line_shader;
        Game_Asset opaque_chunk_shader;
        Game_Asset transparent_chunk_shader;
        Game_Asset screen_shader;
        Game_Asset quad_shader;

        Game_Asset fira_code_font;
        Game_Asset noto_mono_font;
        Game_Asset consolas_mono_font;
        Game_Asset liberation_mono_font;
    };

    void load_game_assets(Game_Assets *assets);
}