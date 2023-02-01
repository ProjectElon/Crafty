#pragma once

#include "core/common.h"

namespace minecraft {

    enum TextureFormat : u8
    {
        TextureFormat_RGB8,
        TextureFormat_RGBA8,
        TextureFormat_R8,
        TextureFormat_RGBA16F,
        TextureFormat_Depth24,
        TextureFormat_Stencil8,
    };

    enum TextureUsage : u8
    {
        TextureUsage_None,
        TextureUsage_SpriteSheet,
        TextureUsage_UI,
        TextureUsage_Font,
        TextureUsage_ColorAttachment,
        TextureUsage_DepthAttachment
    };

    enum TextureWrapMode : u32;

    enum TextureFilterMode : u32;

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
        u32             handle;
        u32             width;
        u32             height;
        TextureFormat   format;
        TextureUsage    usage;
        TextureWrapMode wrap_mode_x;
        TextureWrapMode wrap_mode_y;
        TextureFilterMode min_filter;
        TextureFilterMode mag_filter;
    };

    bool initialize_texture(Opengl_Texture *texture,
                            u8             *data,
                            u32             width,
                            u32             height,
                            TextureFormat   format,
                            TextureUsage    usage = TextureUsage_None);

    bool load_texture(Opengl_Texture *texture,
                      const char     *file_path,
                      TextureUsage    usage = TextureUsage_None);

    void set_texture_params_based_on_usage(Opengl_Texture *texture, TextureUsage usage);

    void free_texture(Opengl_Texture *texture);

    void bind_texture(Opengl_Texture *texture, u32 texture_slot);

    void set_texture_wrap_x(Opengl_Texture *texture, TextureWrapMode wrap_mode);
    void set_texture_wrap_y(Opengl_Texture *texture, TextureWrapMode wrap_mode);
    void set_texture_wrap(Opengl_Texture *texture,
                          TextureWrapMode wrap_mode_x,
                          TextureWrapMode wrap_mode_y);

    void set_texture_min_filtering(Opengl_Texture *texture, TextureFilterMode filter);
    void set_texture_mag_filtering(Opengl_Texture *texture, TextureFilterMode filter);
    void set_texture_filtering(Opengl_Texture *texture,
                               TextureFilterMode min_filter,
                               TextureFilterMode mag_filter);

    u32 texture_format_to_opengl_texture_format(TextureFormat format);
    u32 texture_format_to_opengl_internal_format(TextureFormat format);
}