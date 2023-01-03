#include "opengl_shader.h"
#include <glad/glad.h>

namespace minecraft {

    const char* shader_signature[ShaderType_Count]
    {
        "#vertex",
        "#fragment"
    };

    struct shader_source_segment
    {
        u32 start;
        u32 count;
    };

    // todo(harlequin): move to string utils
    static bool peek_string_buffer(char *a, const char *b)
    {
        u32 count = strlen(b);
        for (u32 i = 0; i < count && *a; ++i, ++a)
        {
            if (b[i] != *a)
            {
                return false;
            }
        }
        return true;
    }

    static const char* convert_glshader_type_to_cstring(u32 shader_Type)
    {
        switch (shader_Type)
        {
            case GL_VERTEX_SHADER:
            {
                return shader_signature[ShaderType_Vertex];
            }
            break;

            case GL_FRAGMENT_SHADER:
            {
                return shader_signature[ShaderType_Fragment];
            }
            break;
        }

        return "unknown";
    }

    static bool compile_shader(const char *file_name,
                               u32 shader_type,
                               const char *shader_source,
                               u32 shader_source_count,
                               u32 *out_shader_id)
    {
        *out_shader_id = 0;

        u32 shader_id = glCreateShader(shader_type);
        int32_t lengths[] = { (int32_t)shader_source_count };
        glShaderSource(shader_id, 1, &shader_source, lengths);
        glCompileShader(shader_id);

        int32_t success;
        glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);

        if (!success)
        {
            char info_log[512];
            glGetShaderInfoLog(shader_id, sizeof(info_log), NULL, info_log);
            fprintf(stderr, "[ERROR]: failed to compile %s shader at %s\n%s\n", convert_glshader_type_to_cstring(shader_type), file_name, info_log);
            return false;
        }

        fprintf(stderr, "[TRACE]: %s shader at %s compiled successfully\n", convert_glshader_type_to_cstring(shader_type), file_name);
        *out_shader_id = shader_id;
        return true;
    }

    void bind_shader(Opengl_Shader *shader)
    {
        glUseProgram(shader->program_id);
    }

    void destroy_shader(Opengl_Shader *shader)
    {
        glDeleteProgram(shader->program_id);
        shader->program_id = 0;
    }

    i32 get_uniform_location(Opengl_Shader *shader, const char *uniform_name)
    {
        i32 uniform_location = glGetUniformLocation(shader->program_id, uniform_name);
        return uniform_location;
    }

    void set_uniform_bool(Opengl_Shader *shader,
                          const char *uniform_name,
                          bool value)
    {
        i32 uniform_location = glGetUniformLocation(shader->program_id, uniform_name);
        glUniform1i(uniform_location, value);
    }

    void set_uniform_i32(Opengl_Shader *shader,
                         const char *uniform_name,
                         i32 value)
    {
        i32 location = get_uniform_location(shader, uniform_name);
        glUniform1i(location, value);
    }

    void set_uniform_i32_array(Opengl_Shader *shader,
                               const char *uniform_name,
                               i32* values,
                               u32 count)
    {
        i32 location = get_uniform_location(shader, uniform_name);
        glUniform1iv(location, count, values);
    }


    void set_uniform_ivec2(Opengl_Shader *shader,
                           const char *uniform_name,
                           i32 value0,
                           i32 value1)
    {
        i32 location = get_uniform_location(shader, uniform_name);
        glUniform2i(location, value0, value1);
    }

    void set_uniform_ivec3(Opengl_Shader *shader,
                           const char *uniform_name,
                           i32 value0,
                           i32 value1,
                           i32 value2)
    {
        i32 location = get_uniform_location(shader, uniform_name);
        glUniform3i(location, value0, value1, value2);
    }

    void set_uniform_f32(Opengl_Shader *shader,
                         const char *uniform_name,
                         float value)
    {
        i32 location = get_uniform_location(shader, uniform_name);
        glUniform1f(location, value);
    }

    void set_uniform_vec2(Opengl_Shader *shader,
                          const char *uniform_name,
                          float value0,
                          float value1)
    {
        i32 location = get_uniform_location(shader, uniform_name);
        glUniform2f(location, value0, value1);
    }

    void set_uniform_vec3(Opengl_Shader *shader,
                          const char *uniform_name,
                          float value0,
                          float value1,
                          float value2)
    {
        i32 location = get_uniform_location(shader, uniform_name);
        glUniform3f(location, value0, value1, value2);
    }

    void set_uniform_vec4(Opengl_Shader *shader,
                          const char *uniform_name,
                          float value0,
                          float value1,
                          float value2,
                          float value3)
    {
        i32 location = get_uniform_location(shader, uniform_name);
        glUniform4f(location, value0, value1, value2, value3);
    }

    void set_uniform_mat3(Opengl_Shader *shader, const char *uniform_name, f32 *matrix)
    {
        i32 location = get_uniform_location(shader, uniform_name);
        glUniformMatrix3fv(location, 1, GL_FALSE, matrix);
    }

    void set_uniform_mat4(Opengl_Shader *shader, const char *uniform_name, f32 *matrix)
    {
        i32 location = get_uniform_location(shader, uniform_name);
        glUniformMatrix4fv(location, 1, GL_FALSE, matrix);
    }

    bool load_shader(Opengl_Shader *shader,
                     const char *file_name)
    {
        // todo(harlequin): move to platform
        // always read text in binary mode
        FILE *file_handle = fopen(file_name, "rb");

        if (!file_handle)
        {
            fprintf(stderr, "[ERROR]: unable to load file %s\n", file_name);
            return false;
        }

        fseek(file_handle, 0, SEEK_END);
        u32 file_size = ftell(file_handle);
        fseek(file_handle, 0, SEEK_SET);

        char *contents = (char*)malloc(file_size + 1);

        if (!contents)
        {
            fprintf(stderr, "[ERROR]: failed to allocate the necessary memory to read file %s\n", file_name);
            return false;
        }

        fread(contents, file_size, 1, file_handle);
        contents[file_size] = '\0';
        fclose(file_handle);

        shader_source_segment segments[ShaderType_Count] = {};

        for (u32 i = 0; i < file_size; i++)
        {
            if (contents[i] == '#')
            {
                for (u32 j = 0; j < ShaderType_Count; j++)
                {
                    if (peek_string_buffer(contents + i, shader_signature[j]))
                    {
                        segments[j].start = i + strlen(shader_signature[j]);
                        i += strlen(shader_signature[j]) - 1;
                        break;
                    }
                }
            }
        }

        segments[ShaderType_Vertex].count = segments[ShaderType_Fragment].start  - segments[ShaderType_Vertex].start - strlen(shader_signature[ShaderType_Fragment]);
        segments[ShaderType_Fragment].count = file_size - segments[ShaderType_Fragment].start;

        u32 vertex_shader_id;
        bool is_vertex_compiled = compile_shader(file_name, GL_VERTEX_SHADER, contents + segments[ShaderType_Vertex].start, segments[ShaderType_Vertex].count, &vertex_shader_id);

        u32 fragment_shader_id;
        bool is_fragment_compiled = compile_shader(file_name, GL_FRAGMENT_SHADER, contents + segments[ShaderType_Fragment].start, segments[ShaderType_Fragment].count, &fragment_shader_id);

        if (!is_vertex_compiled || !is_fragment_compiled)
        {
            return false;
        }

        shader->program_id = glCreateProgram();
        glAttachShader(shader->program_id, vertex_shader_id);
        glAttachShader(shader->program_id, fragment_shader_id);
        glLinkProgram(shader->program_id);

        i32 success;
        glGetProgramiv(shader->program_id, GL_LINK_STATUS, &success);

        if (!success)
        {
            char info_log[512];
            glGetProgramInfoLog(shader->program_id, sizeof(info_log), NULL, info_log);
            fprintf(stderr, "[ERROR]: failed to link shader program at %s\n%s", file_name, info_log);
            return false;
        }

        fprintf(stderr, "[TRACE]: shader program at %s linked successfully\n", file_name);

        glDeleteShader(vertex_shader_id);
        glDeleteShader(fragment_shader_id);
        free(contents);

        return true;
    }
}