#include "opengl_texture.h"

#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace minecraft {

    bool Opengl_Texture::load_from_file(const char *file_path)
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

        this->width = width;
        this->height = height;

        u32 opengl_texture_format;
        u32 opengl_internal_format;

        if (channel_count == 3)
        {
            this->format = TextureFormat_RGB;
            opengl_internal_format = GL_RGB8;
            opengl_texture_format = GL_RGB;
        }
        else if (channel_count == 4)
        {
            this->format = TextureFormat_RGBA;
            opengl_internal_format = GL_RGBA8;
            opengl_texture_format = GL_RGBA;
        }

        glGenTextures(1, &this->id);
        glBindTexture(GL_TEXTURE_2D, this->id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glTexImage2D(GL_TEXTURE_2D, 0, opengl_internal_format, width, height, 0, opengl_texture_format, GL_UNSIGNED_BYTE, data);
        // glGenerateMipmap(GL_TEXTURE_2D);

        stbi_image_free(data);

        fprintf(stderr, "[TRACE]: texture at: %s loaded successfully \n", file_path);

        return true;
    }

    void Opengl_Texture::bind(u32 texture_slot)
    {
        glActiveTexture(GL_TEXTURE0 + texture_slot);
        glBindTexture(GL_TEXTURE_2D, this->id);
    }
}