#pragma once

#include "core/common.h"

namespace minecraft {

    enum TextureFormat : u8
    {
        TextureFormat_RGB,
        TextureFormat_RGBA
    };

    enum TextureUsage : u8
    {
        TextureUsage_SpriteSheet,
        TextureUsage_UI,
        TextureUsage_Font
    };

    enum AnisotropicFiltering : u8
    {
        AnisotropicFiltering_None,
        AnisotropicFiltering_1X,
        AnisotropicFiltering_2X,
        AnisotropicFiltering_4X,
        AnisotropicFiltering_8X,
        AnisotropicFiltering_16X
    };

    struct Opengl_Texture
    {
        u32           id;
        u32           width;
        u32           height;
        TextureFormat format;
        TextureUsage  usage;
    };

    bool initialize_texture(Opengl_Texture *texture,
                            u8             *data,
                            u32             width,
                            u32             height,
                            TextureFormat   format,
                            TextureUsage    usage);

    bool load_texture(Opengl_Texture *texture,
                      const char     *file_path,
                      TextureUsage    usage);

    void free_texture(Opengl_Texture *texture);

    void set_texture_usage(Opengl_Texture *texture, TextureUsage usage);

    void bind_texture(Opengl_Texture *texture, u32 texture_slot);

    u32 texture_format_to_opengl_texture_format(TextureFormat format);
    u32 texture_format_to_opengl_internal_format(TextureFormat format);
}