#pragma once

#include "core/common.h"
#include "containers/string.h"

#include "renderer/opengl_texture.h"

#include <glm/glm.hpp>
#include <stb/stb_truetype.h>

namespace minecraft {

    struct Bitmap_Font
    {
        stbtt_packedchar glyphs[256];
        Opengl_Texture   atlas;
        i32              char_height;
        i32              size_in_pixels;
    };

    bool load_font(Bitmap_Font *font,
                   const char *file_path,
                   i32 size_in_pixels);

    glm::vec2 get_string_size(Bitmap_Font *font, String8 text);
}
