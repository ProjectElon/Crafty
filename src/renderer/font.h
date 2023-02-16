#pragma once

#include "core/common.h"
#include "containers/string.h"

#include "renderer/opengl_texture.h"

#include <glm/glm.hpp>
#include <stb/stb_truetype.h>

namespace minecraft {

    struct Memory_Arena;
    struct Temprary_Memory_Arena;

    struct Bitmap_Font
    {
        u8              *font_data;
        Opengl_Texture   atlas;
        stbtt_packedchar glyphs[256];
        f32              char_height;
        i32              size_in_pixels;
    };

    bool load_font(Bitmap_Font  *font,
                   const char   *file_path,
                   i32           size_in_pixels,
                   Memory_Arena *arena);

    bool set_font_size(Bitmap_Font           *font,
                       i32                    size_in_pixels,
                       Temprary_Memory_Arena *temp_arena);

    glm::vec2 get_string_size(Bitmap_Font *font,
                              String8 text);
}
