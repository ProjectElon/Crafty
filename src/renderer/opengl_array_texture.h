#pragma once

#include "core/common.h"
#include "opengl_texture.h"

namespace minecraft {

    struct Opengl_Array_Texture
    {
        u32           handle;
        u32           width;
        u32           height;
        u32           count;
        TextureFormat format;
    };

    bool initialize_array_texture(Opengl_Array_Texture *array_texture,
                                  u32 width,
                                  u32 height,
                                  u32 count,
                                  TextureFormat format,
                                  bool mipmapping = false);

    void set_image_at(Opengl_Array_Texture *array_texture, const void *pixels, u32 index);
    void bind_texture(Opengl_Array_Texture *array_texture, u32 texture_slot);

    void generate_mipmaps(Opengl_Array_Texture *array_texture);
    void set_anisotropic_filtering_level(Opengl_Array_Texture *array_texture, AnisotropicFiltering anisotropic_filtering = AnisotropicFiltering_None);

}