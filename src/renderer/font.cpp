#include "renderer/font.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

#include <glm/glm.hpp>

namespace minecraft {

    bool Bitmap_Font::load_from_file(const char *file_path, i32 size_in_pixels)
    {
        this->size_in_pixels = size_in_pixels;
        // todo(harlequin): move to platform
        // always read text in binary mode
        FILE *file_handle = fopen(file_path, "rb");

        if (!file_handle)
        {
            fprintf(stderr, "[ERROR]: failed to load font at file %s\n", file_path);
            return false;
        }

        fseek(file_handle, 0, SEEK_END);
        u32 file_size = ftell(file_handle);
        fseek(file_handle, 0, SEEK_SET);

        u8 *font = new u8[file_size];
        defer { delete[] font; };
        fread(font, file_size, 1, file_handle);
        fclose(file_handle);

        stbtt_fontinfo font_info;
        stbtt_InitFont(&font_info, font, 0);
        f32 scale = stbtt_ScaleForPixelHeight(&font_info, size_in_pixels);
        i32 ascent;
        stbtt_GetFontVMetrics(&font_info, &ascent, 0, 0);
        this->char_height = (i32)(ascent * scale);

        char first_char = ' ';
        char last_char = '~';
        i32 char_count = last_char - first_char + 1;
        i32 texture_width  = 2048;
        i32 texture_height = 2048;
        i32 over_sample_x = 8;
        i32 over_sample_y = 8;

        u8 *bitmap = new u8[texture_width * texture_height];
        defer { delete[] bitmap; };
        
        stbtt_pack_context context;
        stbtt_PackSetOversampling(&context, over_sample_x, over_sample_y);

        if (!stbtt_PackBegin(&context, bitmap, texture_width, texture_height, 0, 1, nullptr))
        {
            fprintf(stderr, "[ERROR]: failed to initialize font at file: %s\n", file_path);
            return false;
        }

        if (!stbtt_PackFontRange(&context,
                                 font,
                                 0,
                                 size_in_pixels,
                                 first_char,
                                 char_count,
                                 this->glyphs))
        {
            fprintf(stderr, "[ERROR]: failed to pack font at file: %s\n", file_path);
            return false;
        }

        stbtt_PackEnd(&context);

        u32 *pixels = new u32[texture_width * texture_height];
        defer { delete[] pixels; };

        i32 pixel_count = texture_width * texture_height;
        for (u32 i = 0; i < pixel_count; ++i)
        {
            u32 alpha = bitmap[i];
            pixels[i] = alpha | (alpha << 8) | (alpha << 16) | (alpha << 24);
        }

        bool success = initialize_texture(&this->atlas, (u8*)pixels, texture_width, texture_height, TextureFormat_RGBA, TextureUsage_Font);

        if (success)
        {
            fprintf(stderr, "[TRACE]: font at file: %s loaded\n", file_path);
        }

        return success;
    }

    glm::vec2 Bitmap_Font::get_string_size(String8 text)
    {
        glm::vec2 cursor = { 0.0f, 0.0f };
        glm::vec2 size = { 0.0f, this->char_height };

        for (i32 i = 0; i < text.count; i++)
        {
            Assert(text.data[i] >= ' ' && text.data[i] <= '~');

            stbtt_aligned_quad quad;
            stbtt_GetPackedQuad(this->glyphs,
                                this->atlas.width,
                                this->atlas.height,
                                text.data[i] - ' ',
                                &cursor.x,
                                &cursor.y,
                                &quad,
                                1); // 1 for opengl, 0 for dx3d
        }

        size.x = cursor.x;
        return size;
    }

    glm::vec2 Bitmap_Font::get_string_size(const std::string &text) // @Temprary
    {
        glm::vec2 cursor = { 0.0f, 0.0f };
        glm::vec2 size = { 0.0f, this->char_height };

        for (i32 i = 0; i < text.length(); i++)
        {
            Assert(text[i] >= ' ' && text[i] <= '~');

            stbtt_aligned_quad quad;
            stbtt_GetPackedQuad(this->glyphs,
                                this->atlas.width,
                                this->atlas.height,
                                text[i] - ' ',
                                &cursor.x,
                                &cursor.y,
                                &quad,
                                1); // 1 for opengl, 0 for dx3d
        }

        size.x = cursor.x;
        return size;
    }
}