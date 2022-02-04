#pragma once

#include "core/common.h"
#include "renderer/opengl_texture.h"

#include <vector>
#include <string>

namespace minecraft {

    struct Pixel
    {
        u8 r;
        u8 g;
        u8 b;
        u8 a;
    };

    struct Texture_Packer
    {
        static bool pack_textures(
            std::vector<std::string> &paths,
            const std::string &image_output_path,
            const std::string &meta_output_path,
            const std::string &header_output_path);
    };
}