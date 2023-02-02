#pragma once

#include "core/common.h"
#include "game/math.h"
#include "opengl_texture.h"

namespace minecraft {

    struct Memory_Arena;

    struct Texture_Coords
    {
        glm::vec2 offset;
        glm::vec2 scale;
    };

    struct Opengl_Texture_Atlas
    {
        Opengl_Texture *texture;
        u32             sub_texture_count;
        Rectangle2i    *sub_texture_rectangles;
        Texture_Coords *sub_texture_texture_coords;
    };

    bool initialize_texture_atlas(Opengl_Texture_Atlas *atlas,
                                  Opengl_Texture       *texture,
                                  u32                   sub_texture_count,
                                  Rectangle2i          *sub_texture_rectangles,
                                  Memory_Arena         *arena);

    const Texture_Coords* get_sub_texture_coords(Opengl_Texture_Atlas *atlas,
                                                 u32 sub_texture_index);
}