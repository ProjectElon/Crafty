#pragma once

#include "core/common.h"

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
    };

    bool load_shader(Opengl_Shader *shader, const char *file_name);

    void bind_shader(Opengl_Shader *shader);
    void destroy_shader(Opengl_Shader *shader);

    i32 get_uniform_location(Opengl_Shader *shader, const char *uniform_name);

    void set_uniform_bool(Opengl_Shader *shader, const char *uniform_name, bool value);

    void set_uniform_i32(Opengl_Shader *shader, const char *uniform_name, i32 value);
    void set_uniform_i32_array(Opengl_Shader *shader, const char *uniform_name, i32* values, u32 count);

    void set_uniform_ivec2(Opengl_Shader *shader, const char *uniform_name, i32 value0, i32 value1);
    void set_uniform_ivec3(Opengl_Shader *shader, const char *uniform_name, i32 value0, i32 value1, i32 value2);

    void set_uniform_f32(Opengl_Shader *shader,  const char *uniform_name, f32 value);
    void set_uniform_vec2(Opengl_Shader *shader, const char *uniform_name, f32 value0, f32 value1);
    void set_uniform_vec3(Opengl_Shader *shader, const char *uniform_name, f32 value0, f32 value1, f32 value2);
    void set_uniform_vec4(Opengl_Shader *shader, const char *uniform_name, f32 value0, f32 value1, f32 value2, f32 value3);
    void set_uniform_mat3(Opengl_Shader *shader, const char *uniform_name, f32 *matrix);
    void set_uniform_mat4(Opengl_Shader *shader, const char *uniform_name, f32 *matrix);
}