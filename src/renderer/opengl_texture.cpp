#include "opengl_texture.h"

#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace minecraft {

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

        glCreateTextures(GL_TEXTURE_2D, 1, &texture->handle);
        Assert(texture->handle);

        glTextureParameteri(texture->handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(texture->handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        set_texture_usage(texture, usage);

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

    void free_texture(Opengl_Texture *texture)
    {
        glDeleteTextures(1, &texture->handle);
        texture->handle = 0;
    }

    void set_texture_usage(Opengl_Texture *texture, TextureUsage usage)
    {
        texture->usage = usage;

        switch (usage)
        {
            case TextureUsage_SpriteSheet:
            {
                glTextureParameteri(texture->handle, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTextureParameteri(texture->handle, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            } break;

            case TextureUsage_UI:
            {
                glTextureParameteri(texture->handle, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTextureParameteri(texture->handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            } break;

            case TextureUsage_Font:
            {
                glTextureParameteri(texture->handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTextureParameteri(texture->handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            }
            break;

            case TextureUsage_ColorAttachment:
            case TextureUsage_DepthAttachment:
            {
                glTextureParameteri(texture->handle, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTextureParameteri(texture->handle, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            } break;
        }
    }

    void bind_texture(Opengl_Texture *texture, u32 texture_slot)
    {
        glBindTextureUnit(texture_slot, texture->handle);
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