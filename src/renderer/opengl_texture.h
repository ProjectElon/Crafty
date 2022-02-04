#pragma once

#include "core/common.h"

namespace minecraft {

    enum TextureFormat
    {
        TextureFormat_RGB,
        TextureFormat_RGBA
    };

    struct Opengl_Texture
    {
        u32 id;
        u32 width;
        u32 height;
        TextureFormat format;

        bool load_from_file(const char *file_path);
        void bind(u32 texture_slot);
    };
}