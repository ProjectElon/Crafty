#pragma once

#include "core/common.h"

#include "renderer/opengl_texture.h"

#include <glm/glm.hpp>
#include <stb/stb_truetype.h>

namespace minecraft {

    struct Bitmap_Font
    {
        stbtt_packedchar glyphs[256];
        Opengl_Texture atlas;
        i32 char_height;

        bool load_from_file(const char *file_path, i32 size_in_pixels);
        glm::vec2 get_string_size(const std::string& text);
    };
}
