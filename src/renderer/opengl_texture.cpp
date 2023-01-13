#include "opengl_texture.h"

#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace minecraft {

    static u32 texture_format_to_opengl_texture_format(TextureFormat format)
    {
        switch (format)
        {
        case TextureFormat_RGB:
            return GL_RGB;
            break;
        case TextureFormat_RGBA:
            return GL_RGBA;
            break;
        default:
            Assert(false && "unsupported texture format");
            break;
        }

        return 0;
    }

    static u32 texture_format_to_opengl_internal_format(TextureFormat format)
    {
        switch (format)
        {
        case TextureFormat_RGB:
            return GL_RGB8;
            break;
        case TextureFormat_RGBA:
            return GL_RGBA8;
            break;
        default:
            Assert(false && "unsupported internal format");
            break;
        }

        return 0;
    }

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

        glGenTextures(1, &texture->id);
        glBindTexture(GL_TEXTURE_2D, texture->id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        set_texture_usage(texture, usage);

        i32 internal_format = texture_format_to_opengl_internal_format(format);
        i32 texture_format  = texture_format_to_opengl_texture_format(format);

        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     internal_format,
                     width,
                     height,
                     0,
                     texture_format,
                     GL_UNSIGNED_BYTE,
                     data);

        glBindTexture(GL_TEXTURE_2D, 0);

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
            texture_format = TextureFormat_RGB;
        }
        else if (channel_count == 4)
        {
            texture_format = TextureFormat_RGBA;
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
        glDeleteTextures(1, &texture->id);
        texture->id = 0;
    }

    void set_texture_usage(Opengl_Texture *texture, TextureUsage usage)
    {
        texture->usage = usage;
        glBindTexture(GL_TEXTURE_2D, texture->id);

        switch (usage)
        {
            case TextureUsage_SpriteSheet:
            {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            } break;

            case TextureUsage_UI:
            {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            } break;

            case TextureUsage_Font:
            {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            }
            break;
        }
    }

    void bind_texture(Opengl_Texture *texture, u32 texture_slot)
    {
        glActiveTexture(GL_TEXTURE0 + texture_slot);
        glBindTexture(GL_TEXTURE_2D, texture->id);
    }
}