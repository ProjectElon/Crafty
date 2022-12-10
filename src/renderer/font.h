#pragma once

#include "core/common.h"
#include "containers/string.h"

#include "renderer/opengl_texture.h"

#include <glm/glm.hpp>
#include <stb/stb_truetype.h>

#include <string> // @Temprary

namespace minecraft {

    struct Bitmap_Font
    {
        stbtt_packedchar glyphs[256];
        Opengl_Texture atlas;
        i32 char_height;
        i32 size_in_pixels;

        bool load_from_file(const char *file_path, i32 size_in_pixels);
        glm::vec2 get_string_size(String8 text);

        glm::vec2 get_string_size(const std::string &text); // @Temprary
    };
}
