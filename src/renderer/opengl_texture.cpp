#include "opengl_texture.h"

#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace minecraft {

    enum TextureWrapMode : u32
    {
        TextureWrapMode_Repeat = GL_REPEAT,
        TextureWrapMode_Clamp  = GL_CLAMP_TO_EDGE
    };

    enum TextureFilterMode : u32
    {
        TextureFilterMode_Nearest = GL_NEAREST,
        TextureFilterMode_Linear  = GL_LINEAR,
    };

    bool initialize_texture(Opengl_Texture *texture,
                            u8             *data,
                            u32             width,
                            u32             height,
                            TextureFormat   format,
                            TextureUsage    usage)
    {
        texture->width  = width;
        texture->height = height;
        texture->format = format;
        texture->usage  = usage;

        glCreateTextures(GL_TEXTURE_2D, 1, &texture->handle);
        Assert(texture->handle);

        set_texture_wrap(texture, TextureWrapMode_Clamp, TextureWrapMode_Clamp);
        set_texture_params_based_on_usage(texture, usage);

        i32 internal_format = texture_format_to_opengl_internal_format(format);
        i32 texture_format  = texture_format_to_opengl_texture_format(format);

        i32 pixel_data_type = GL_UNSIGNED_BYTE;

        if (format == TextureFormat_RGBA16F)
        {
            pixel_data_type = GL_HALF_FLOAT;
        }
        else if (format == TextureFormat_R8)
        {
            pixel_data_type = GL_FLOAT;
        }

        glTextureStorage2D(texture->handle, 1, internal_format, width, height);
        if (data)
        {
            glTextureSubImage2D(texture->handle,
                                0,
                                0,
                                0,
                                width,
                                height,
                                texture_format,
                                pixel_data_type,
                                data);
        }
        return true;
    }

    bool load_texture(Opengl_Texture *texture,
                      const char     *file_path,
                      TextureUsage    usage)
    {
        i32 width;
        i32 height;
        i32 channel_count;
        stbi_set_flip_vertically_on_load(true);
        u8* data = stbi_load(file_path, &width, &height, &channel_count, 0);

        if (!data)
        {
            fprintf(stderr, "[ERROR]: failed to load texture at: %s\n", file_path);
            return false;
        }

        defer { stbi_image_free(data); };

        u32 opengl_texture_format;
        u32 opengl_internal_format;
        u8 texture_format;

        if (channel_count == 3)
        {
            texture_format = TextureFormat_RGB8;
        }
        else if (channel_count == 4)
        {
            texture_format = TextureFormat_RGBA8;
        }
        else
        {
            Assert(false && "unsupported channel count");
        }

        bool success = initialize_texture(texture,
                                          data,
                                          (u32)width,
                                          (u32)height,
                                          (TextureFormat)texture_format,
                                          usage);

        if (success)
        {
            fprintf(stderr, "[TRACE]: texture at file: %s loaded\n", file_path);
        }

        return success;
    }

    void set_texture_params_based_on_usage(Opengl_Texture *texture, TextureUsage usage)
    {
        texture->usage = usage;

        switch (usage)
        {
            case TextureUsage_None:
            {
                set_texture_filtering(texture, TextureFilterMode_Linear, TextureFilterMode_Nearest);
            } break;

            case TextureUsage_SpriteSheet:
            {
                set_texture_filtering(texture, TextureFilterMode_Nearest, TextureFilterMode_Nearest);
            } break;

            case TextureUsage_UI:
            {
                set_texture_filtering(texture, TextureFilterMode_Nearest, TextureFilterMode_Linear);
            } break;

            case TextureUsage_Font:
            {
                set_texture_filtering(texture, TextureFilterMode_Linear, TextureFilterMode_Linear);
            }
            break;

            case TextureUsage_ColorAttachment:
            case TextureUsage_DepthAttachment:
            {
                set_texture_filtering(texture, TextureFilterMode_Nearest, TextureFilterMode_Nearest);
            } break;
        }
    }

    void free_texture(Opengl_Texture *texture)
    {
        glDeleteTextures(1, &texture->handle);
        texture->handle = 0;
    }

    void bind_texture(Opengl_Texture *texture, u32 texture_slot)
    {
        glBindTextureUnit(texture_slot, texture->handle);
    }

    void set_texture_wrap_x(Opengl_Texture *texture, TextureWrapMode wrap_mode)
    {
        texture->wrap_mode_x = wrap_mode;
        glTextureParameteri(texture->handle, GL_TEXTURE_WRAP_S, wrap_mode);
    }

    void set_texture_wrap_y(Opengl_Texture *texture,
                            TextureWrapMode wrap_mode)
    {
        texture->wrap_mode_y = wrap_mode;
        glTextureParameteri(texture->handle, GL_TEXTURE_WRAP_T, wrap_mode);
    }

    void set_texture_wrap(Opengl_Texture *texture,
                          TextureWrapMode wrap_mode_x,
                          TextureWrapMode wrap_mode_y)
    {
        texture->wrap_mode_x = wrap_mode_x;
        texture->wrap_mode_y = wrap_mode_y;
        glTextureParameteri(texture->handle, GL_TEXTURE_WRAP_S, wrap_mode_x);
        glTextureParameteri(texture->handle, GL_TEXTURE_WRAP_T, wrap_mode_y);
    }

    void set_texture_min_filtering(Opengl_Texture *texture, TextureFilterMode filter)
    {
        texture->min_filter = filter;
        glTextureParameteri(texture->handle, GL_TEXTURE_MIN_FILTER, filter);
    }

    void set_texture_mag_filtering(Opengl_Texture *texture, TextureFilterMode filter)
    {
        texture->mag_filter = filter;
        glTextureParameteri(texture->handle, GL_TEXTURE_MAG_FILTER, filter);
    }

    void set_texture_filtering(Opengl_Texture   *texture,
                               TextureFilterMode min_filter,
                               TextureFilterMode mag_filter)
    {
        texture->min_filter = min_filter;
        texture->mag_filter = mag_filter;
        glTextureParameteri(texture->handle, GL_TEXTURE_MIN_FILTER, min_filter);
        glTextureParameteri(texture->handle, GL_TEXTURE_MAG_FILTER, mag_filter);
    }

    u32 texture_format_to_opengl_texture_format(TextureFormat format)
    {
        switch (format)
        {
            case TextureFormat_RGB8:
            {
                return GL_RGB;
            } break;

            case TextureFormat_RGBA8:
            {
                return GL_RGBA;
            } break;

            case TextureFormat_R8:
            {
                return GL_RED;
            } break;

            case TextureFormat_RGBA16F:
            {
                return GL_RGBA;
            } break;

            case TextureFormat_Depth24:
            {
                return GL_DEPTH_COMPONENT;
            } break;

            case TextureFormat_Stencil8:
            {
                return GL_STENCIL_INDEX;
            } break;

            default:
            {
                Assert(false && "unsupported texture format");
            } break;
        }

        return 0;
    }

    u32 texture_format_to_opengl_internal_format(TextureFormat format)
    {
        switch (format)
        {
            case TextureFormat_RGB8:
            {
                return GL_RGB8;
            } break;

            case TextureFormat_RGBA8:
            {
                return GL_RGBA8;
            } break;

            case TextureFormat_R8:
            {
                return GL_R8;
            } break;

            case TextureFormat_RGBA16F:
            {
                return GL_RGBA16F;
            } break;

            case TextureFormat_Depth24:
            {
                return GL_DEPTH_COMPONENT24;
            } break;

            case TextureFormat_Stencil8:
            {
                return GL_STENCIL_INDEX8;
            } break;

            default:
            {
                Assert(false && "unsupported internal format");
            } break;
        }

        return 0;
    }
}