#include "opengl_array_texture.h"

#include <glad/glad.h>

namespace minecraft {

    bool initialize_array_texture(Opengl_Array_Texture *array_texture,
                                  u32 width,
                                  u32 height,
                                  u32 count,
                                  TextureFormat format,
                                  bool mipmapping)
    {
        Assert(width);
        Assert(height);
        Assert(count);

        array_texture->handle = 0;
        glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &array_texture->handle);
        Assert(array_texture->handle);
        u32 mipmapping_levels = mipmapping ? 1 + (u32)floor(log2(Max(width, height))) : 0;
        u32 internal_format = texture_format_to_opengl_internal_format(format);
        glTextureStorage3D(array_texture->handle,
                           mipmapping_levels,
                           internal_format,
                           width,
                           height,
                           count);

        glTextureParameteri(array_texture->handle,
                            GL_TEXTURE_MIN_FILTER,
                            mipmapping ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST);

        glTextureParameteri(array_texture->handle,
                            GL_TEXTURE_MAG_FILTER,
                            GL_NEAREST);

        glTextureParameteri(array_texture->handle,
                            GL_TEXTURE_WRAP_S,
                            GL_CLAMP_TO_EDGE);

        glTextureParameteri(array_texture->handle,
                            GL_TEXTURE_WRAP_T,
                            GL_CLAMP_TO_EDGE);

        array_texture->width  = width;
        array_texture->height = height;
        array_texture->count  = count;
        array_texture->format = format;

        return true;
    }

    void set_image_at(Opengl_Array_Texture *array_texture, const void *pixels, u32 index)
    {
        Assert(array_texture);
        Assert(array_texture->handle);
        Assert(pixels);
        Assert(index >= 0 && index < array_texture->count);
        u32 texture_format = texture_format_to_opengl_texture_format(array_texture->format);
        glTextureSubImage3D(array_texture->handle,
                            0,
                            0, 0, index,
                            array_texture->width,
                            array_texture->height, 1,
                            texture_format,
                            GL_UNSIGNED_BYTE,
                            pixels);
    }

    void bind_texture(Opengl_Array_Texture *array_texture, u32 texture_slot)
    {
        glBindTextureUnit(texture_slot, array_texture->handle);
    }

    void generate_mipmaps(Opengl_Array_Texture *array_texture)
    {
        glGenerateTextureMipmap(array_texture->handle);
    }

    static f32 anisotropic_filtering_to_f32(AnisotropicFiltering anisotropic_filtering)
    {
        f32 anisotropy = 0.0f;

        switch (anisotropic_filtering)
        {
            case AnisotropicFiltering_1X:
            {
                anisotropy = 1.0f;
            }
            break;

            case AnisotropicFiltering_2X:
            {
                anisotropy = 2.0f;
            }
            break;

            case AnisotropicFiltering_4X:
            {
                anisotropy = 4.0f;
            }
            break;

            case AnisotropicFiltering_8X:
            {
                anisotropy = 8.0f;
            }
            break;

            case AnisotropicFiltering_16X:
            {
                anisotropy = 16.0f;
            }
            break;
        }

        f32 max_anisotropy = 0.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropy);

        if (anisotropy > max_anisotropy)
        {
            anisotropy = max_anisotropy;
        }

        return anisotropy;
    }

    void set_anisotropic_filtering_level(Opengl_Array_Texture *array_texture,  AnisotropicFiltering anisotropic_filtering)
    {
        glTextureParameterf(array_texture->handle,
                            GL_TEXTURE_MAX_ANISOTROPY_EXT,
                            anisotropic_filtering_to_f32(anisotropic_filtering));
    }
}
