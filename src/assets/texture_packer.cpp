#include "texture_packer.h"

#include <glm/glm.hpp>
#include <stb/stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include <unordered_map>
#include <algorithm>
#include <filesystem>

// todo(harlequin): to be removed
#include <fstream>
#include <sstream>

namespace minecraft {

    struct Texture_Info
    {
        std::string name;
        Rectanglei rect;
    };

    bool Texture_Packer::pack_textures(
        std::vector< std::string > &paths,
        const std::string          &image_output_path,
        const std::string          &meta_output_path,
        const std::string          &header_output_path)
    {
        u32 texture_count = (u32)paths.size();

        u32 output_width = (u32)sqrt(texture_count * 32 * 32);
        std::vector<Pixel> pixels(output_width);

        u32 current_x = 0;
        u32 current_y = 0;
        u32 line_height = 0;

        // todo(harlequin): repalce with String_Builder
        std::stringstream meta_file_stream;
        std::stringstream texture_enum_stream;
        std::stringstream texture_rect_stream;
        std::stringstream texture_name_stream;

        auto compare = [](const std::string& a, const std::string& b) -> bool
        {
            unsigned char buf[8];

            std::ifstream a_in(a);
            a_in.seekg(16);
            a_in.read(reinterpret_cast<char*>(&buf), 8);
            a_in.close();

            u32 a_height = (buf[4] << 24) + (buf[5] << 16) + (buf[6] << 8) + (buf[7] << 0);

            std::ifstream b_in(b);
            b_in.seekg(16);
            b_in.read(reinterpret_cast<char*>(&buf), 8);
            b_in.close();

            u32 b_height = (buf[4] << 24) + (buf[5] << 16) + (buf[6] << 8) + (buf[7] << 0);

            if (a_height == b_height)
            {
                return a < b;
            }

            return a_height < b_height;
        };

        std::sort(std::begin(paths), std::end(paths), compare);

        u32 texture_id = 0;
        std::vector<Texture_Info> textures(texture_count);

        for (const auto& path : paths)
        {
            i32 width;
            i32 height;
            i32 channels;

            stbi_set_flip_vertically_on_load(false);
            u8* data = stbi_load(path.c_str(), &width, &height, &channels, 4);

            if (!data)
            {
                fprintf(stderr, "[ERROR]: failed to pack texture at file %s\n", path.c_str());
                continue;
            }

            u32 new_x = current_x + width;

            if (new_x >= output_width)
            {
                current_x = 0;
                current_y += line_height;
                line_height = 0;
            }

            if (height > (i32)line_height)
            {
                line_height = height;
                pixels.resize((current_y + line_height + 1) * output_width);
            }

            for (i32 y = 0; y < height; ++y)
            {
                for (i32 x = 0; x < width; x++)
                {
                    i32 cursor_x = x + current_x;
                    i32 cursor_y = y + current_y;
                    Pixel *current_pixel  = &pixels[cursor_y * output_width + cursor_x];
                    u8* current_row_pixel = &data[(y * width + x) * 4];
                    current_pixel->r = *current_row_pixel;
                    current_pixel->g = *(current_row_pixel + 1);
                    current_pixel->b = *(current_row_pixel + 2);
                    current_pixel->a = *(current_row_pixel + 3);
                }
            }

            Rectanglei texture_rect = { (i32)current_x, (i32)current_y, (u32)width, (u32)height };

            std::string texture_name_without_extension = std::filesystem::path(path).filename().stem().string();
            Texture_Info& info = textures[texture_id];
            info.name = texture_name_without_extension;
            info.rect = texture_rect;

            meta_file_stream << texture_name_without_extension << " " << current_x << " " << current_y << " " << width << " " << height << "\n";
            texture_enum_stream << "\tTexture_Id_" << texture_name_without_extension << " = " << texture_id << ",\n";
            texture_rect_stream << "\t{ " << current_x << ", " << current_y << ", " << width << ", " << height << " },\n";
            texture_name_stream << "\"" << texture_name_without_extension << "\",";

            current_x += width;
            texture_id++;

            stbi_image_free(data);
        }

        u32 output_height = current_y + line_height;

        bool success = stbi_write_png(
            image_output_path.c_str(),
            output_width,
            output_height,
            4,
            pixels.data(),
            output_width * sizeof(Pixel));

        if (!success)
        {
            fprintf(stderr, "[ERROR]: Failed to write packed output texture at: %s\n", image_output_path.c_str());
            return false;
        }

        fprintf(
            stderr,
            "[TRACE]: texture at %s created successfully with width = %u, height = %u\n",
            image_output_path.c_str(),
            output_width,
            output_height);

        FILE *locations_file = fopen(meta_output_path.c_str(), "wb");

        if (!locations_file)
        {
            fprintf(stderr, "[ERROR]: Failed to open locations file at: %s\n", meta_output_path.c_str());
            return false;
        }

        defer
        {
            fclose(locations_file);
        };

        const std::string& meta_file_contents = meta_file_stream.str();
        success = fwrite(meta_file_contents.c_str(), meta_file_contents.length(), 1, locations_file) == 1;

        if (!success)
        {
            fprintf(stderr, "[ERROR]: Failed to write meta file at: %s\n", meta_output_path.c_str());
            return false;
        }

        fprintf(stderr, "[TRACE]: meta file create successfully at %s\n", meta_output_path.c_str());

        std::stringstream header_file_stream;
        header_file_stream << "#pragma once\n";
        header_file_stream << "#include <cstdint>\n";
        header_file_stream << "#include <glm/glm.hpp>\n";
        header_file_stream << "#define MC_PACKED_TEXTURE_COUNT " << texture_count << "\n";
        header_file_stream << "namespace minecraft {\n\n";
        header_file_stream << R"(struct Texture_Rect
                                {
                                    uint32_t x;
                                    uint32_t y;
                                    uint32_t width;
                                    uint32_t height;
                                };
                                )";

        header_file_stream << R"(struct UV_Rect
                                {
                                    glm::vec2 bottom_right;
                                    glm::vec2 bottom_left;
                                    glm::vec2 top_left;
                                    glm::vec2 top_right;
                                };
                                )";

        header_file_stream << "enum Texture_Id : uint16_t\n{\n";
        header_file_stream << texture_enum_stream.str();
        header_file_stream << "};\n";

        header_file_stream << "static Texture_Rect texture_rects[MC_PACKED_TEXTURE_COUNT] = \n{\n";
        header_file_stream << texture_rect_stream.str();
        header_file_stream << "};\n";

        header_file_stream << "static const char* texture_names[MC_PACKED_TEXTURE_COUNT] = \n{\n";
        header_file_stream << texture_name_stream.str();
        header_file_stream << "};\n";


        std::stringstream texture_uv_rect_stream;

        for (auto& texture_info : textures)
        {
            UV_Rectangle uv_rect = convert_texture_rect_to_uv_rect(texture_info.rect, output_width, output_height);
            texture_uv_rect_stream << "\t{ ";
            texture_uv_rect_stream << "{ " << uv_rect.bottom_right.x << ", " << uv_rect.bottom_right.y << " }, ";
            texture_uv_rect_stream << "{ " << uv_rect.bottom_left.x  << ", " << uv_rect.bottom_left.y << " }, ";
            texture_uv_rect_stream << "{ " << uv_rect.top_left.x     << ", " << uv_rect.top_left.y << " }, ";
            texture_uv_rect_stream << "{ " << uv_rect.top_right.x    << ", " << uv_rect.top_right.y << " }";
            texture_uv_rect_stream << " },\n";
        }

        header_file_stream << "static UV_Rect texture_uv_rects[MC_PACKED_TEXTURE_COUNT] = \n{\n";
        header_file_stream << texture_uv_rect_stream.str();
        header_file_stream << "};\n";

        header_file_stream << "}\n"; // namespace

        FILE *header_file = fopen(header_output_path.c_str(), "wb");

        if (!header_file)
        {
            fprintf(stderr, "[ERROR]: failed to open header file at %s\n", header_output_path.c_str());
            return false;
        }

        defer
        {
            fclose(header_file);
        };

        success = fwrite(header_file_stream.str().c_str(), header_file_stream.str().length(), 1, header_file) == 1;

        if (!success)
        {
            fprintf(stderr, "[ERROR]: failed to write header file at %s\n", header_output_path.c_str());
            return false;
        }

        fprintf(stderr, "[TRACE]: header file create successfully at %s\n", header_output_path.c_str());

        return true;
    }
}