#pragma once

#include "core/common.h"
#include "game/math.h"
#include "opengl_texture.h"

#define INVALID_SUB_TEXTURE_INDEX (~0u)

namespace minecraft {

    struct Memory_Arena;
    struct Temprary_Memory_Arena;
    struct String8;

    struct Texture_Coords
    {
        glm::vec2 offset;
        glm::vec2 scale;
    };

    struct Opengl_Texture_Atlas
    {
        u32             texture_asset_handle;
        Opengl_Texture *texture;
        u32             sub_texture_count;
        Rectangle2i    *sub_texture_rectangles;
        Texture_Coords *sub_texture_texture_coords;
        String8        *sub_texture_names;
    };

    bool initialize_texture_atlas(Opengl_Texture_Atlas *atlas,
                                  u32                   texture_asset_handle,
                                  u32                   sub_texture_count,
                                  Rectangle2i          *sub_texture_rectangles,
                                  String8              *sub_texture_names,
                                  Memory_Arena         *arena);

    const Texture_Coords* get_sub_texture_coords(Opengl_Texture_Atlas *atlas,
                                                 u32                   sub_texture_index);

    u32 get_sub_texture_index(Opengl_Texture_Atlas *atlas,
                              const String8        &sub_texture_name);

    bool serialize_texture_atlas(Opengl_Texture_Atlas  *atlas,
                                 const char            *file_path);

    bool deserialize_texture_atlas(Opengl_Texture_Atlas *atlas,
                                   const char           *file_path,
                                   Memory_Arena         *arena);
}