#include "renderer/font.h"
#include "memory/memory_arena.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

#include <glm/glm.hpp>

namespace minecraft {

    bool load_font(Bitmap_Font  *font,
                   const char   *file_path,
                   i32           size_in_pixels,
                   Memory_Arena *arena)
    {
        // todo(harlequin): file api
        FILE *file_handle = fopen(file_path, "rb");

        if (!file_handle)
        {
            fprintf(stderr, "[ERROR]: failed to open font file: %s\n", file_path);
            return false;
        }

        defer { fclose(file_handle); };

        fseek(file_handle, 0, SEEK_END);
        u32 file_size = ftell(file_handle);
        fseek(file_handle, 0, SEEK_SET);

        font->font_data   = ArenaPushArrayAlignedZero(arena, u8, file_size);
        size_t read_count = fread(font->font_data, file_size, 1, file_handle);

        if (read_count != 1)
        {
            fprintf(stderr, "[ERROR]: failed to read font file: %s\n", file_path);
            return false;
        }

        Temprary_Memory_Arena temp_arena = begin_temprary_memory_arena(arena);
        bool success = set_font_size(font, size_in_pixels, &temp_arena);
        end_temprary_memory_arena(&temp_arena);

        if (!success)
        {
            fprintf(stderr, "[ERROR]: failed to load font file: %s loaded\n", file_path);
            return false;
        }

        fprintf(stderr, "[TRACE]: font file: %s loaded\n", file_path);
        return success;
    }

    glm::vec2 get_string_size(Bitmap_Font *font,
                              String8 text)
    {
        glm::vec2 cursor = { 0.0f, 0.0f };
        glm::vec2 size   = { 0.0f, font->char_height };

        for (i32 i = 0; i < text.count; i++)
        {
            Assert(text.data[i] >= ' ' && text.data[i] <= '~');

            stbtt_aligned_quad quad;
            stbtt_GetPackedQuad(font->glyphs,
                                font->atlas.width,
                                font->atlas.height,
                                text.data[i] - ' ',
                                &cursor.x,
                                &cursor.y,
                                &quad,
                                1); // 1 for opengl, 0 for dx3d
        }

        size.x = cursor.x;
        return size;
    }

    bool set_font_size(Bitmap_Font           *font,
                       i32                    size_in_pixels,
                       Temprary_Memory_Arena *temp_arena)
    {
        stbtt_fontinfo font_info;
        stbtt_InitFont(&font_info, font->font_data, 0);
        i32 ascent;
        stbtt_GetFontVMetrics(&font_info, &ascent, 0, 0);
        f32 scale = stbtt_ScaleForPixelHeight(&font_info, size_in_pixels);
        font->char_height = (i32)(ascent * scale);
        font->size_in_pixels = size_in_pixels;

        char first_char    = ' ';
        char last_char     = '~';
        i32 char_count     = last_char - first_char + 1;
        i32 texture_width  = 2048;
        i32 texture_height = 2048;
        i32 over_sample_x  = 8;
        i32 over_sample_y  = 8;

        u8 *bitmap = ArenaPushArrayAlignedZero(temp_arena, u8, texture_width * texture_height);

        stbtt_pack_context context;
        stbtt_PackSetOversampling(&context, over_sample_x, over_sample_y);

        if (!stbtt_PackBegin(&context,
                             bitmap,
                             texture_width,
                             texture_height,
                             0,
                             1,
                             nullptr))
        {
            fprintf(stderr, "[ERROR]: failed to initialize font\n");
            return false;
        }

        if (!stbtt_PackFontRange(&context,
                                 font->font_data,
                                 0,
                                 size_in_pixels,
                                 first_char,
                                 char_count,
                                 font->glyphs))
        {
            fprintf(stderr, "[ERROR]: failed to pack font\n");
            return false;
        }

        stbtt_PackEnd(&context);

        i32 pixel_count = texture_width * texture_height;
        u32 *pixels = ArenaPushArrayAligned(temp_arena, u32, pixel_count);

        for (u32 i = 0; i < pixel_count; ++i)
        {
            u32 alpha = bitmap[i];
            pixels[i] = alpha | (alpha << 8) | (alpha << 16) | (alpha << 24);
        }

        if (font->atlas.handle)
        {
            free_texture(&font->atlas);
        }

        bool success = initialize_texture(&font->atlas,
                                          (u8*)pixels,
                                          texture_width,
                                          texture_height,
                                          TextureFormat_RGBA8,
                                          TextureUsage_Font);
        return success;
    }
}