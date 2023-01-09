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

    struct Opengl_Texture
    {
        u32           id;
        u32           width;
        u32           height;
        TextureFormat format;
        TextureUsage  usage;
    };

    bool initialize_texture(Opengl_Texture *texture,
                            u8 *data,
                            u32 width,
                            u32 height,
                            TextureFormat format,
                            TextureUsage usage);

    bool load_texture(Opengl_Texture *texture,
                      const char     *file_path,
                      TextureUsage    usage);

    void set_texture_usage(Opengl_Texture *texture, TextureUsage usage);

    void bind_texture(Opengl_Texture *texture, u32 texture_slot);
}