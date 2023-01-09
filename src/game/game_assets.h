#pragma once

#include "core/common.h"
#include "containers/string.h"

namespace minecraft {

    struct Memory_Arena;

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
}