#pragma once

#include "core/common.h"
#include <unordered_map> // todo(harlequin): data structures

namespace minecraft {

    enum ShaderType
    {
        ShaderType_Vertex   = 0,
        ShaderType_Fragment = 1,
        ShaderType_Count    = 2
    };

    struct Opengl_Shader
    {
        u32 program_id;
        std::unordered_map<const char *, i32> uniform_name_to_location_table;

        bool load_from_file(const char *file_name);

        void use();
        void destroy();

        i32 get_uniform_location(const char *uniform_name);

        void set_uniform_i32(const char *uniform_name, i32 value);
        void set_uniform_f32(const char *uniform_name, f32 value);
        void set_uniform_vec2(const char *uniform_name, f32 value0, f32 value1);
        void set_uniform_vec3(const char *uniform_name, f32 value0, f32 value1, f32 value2);
        void set_uniform_vec4(const char *uniform_name, f32 value0, f32 value1, f32 value2, f32 value3);
        void set_uniform_mat3(const char *uniform_name, f32 *matrix);
        void set_uniform_mat4(const char *uniform_name, f32 *matrix);
    };
}