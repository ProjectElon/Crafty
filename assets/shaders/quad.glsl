#vertex

#version 430 core

layout (location = 0) in vec2 in_position;
layout (location = 1) in vec2 in_uv;

layout (location = 2) in vec2  instance_position;
layout (location = 3) in vec2  instance_scale;
layout (location = 4) in float instance_rotation;

layout (location = 5) in vec4 instance_color;
layout (location = 6) in int  instance_texture_index;
layout (location = 7) in vec2 instance_uv_scale;
layout (location = 8) in vec2 instance_uv_offset;

out vec4 a_color;
out vec2 a_uv;
out flat int a_texture_index;
out vec2 a_uv_scale;
out vec2 a_uv_offset;

uniform mat4 u_projection;

void main()
{
    mat2 rot = mat2(cos(instance_rotation), -sin(instance_rotation),
                    sin(instance_rotation), cos(instance_rotation));

    vec2 position =  in_position * instance_scale * rot;

    gl_Position     = u_projection * vec4(position + instance_position, 0.0f, 1.0f);
    a_uv            = in_uv * instance_uv_scale + instance_uv_offset;
    a_color         = instance_color;
    a_texture_index = instance_texture_index;
}

#fragment

#version 430 core

in vec4 a_color;
in vec2 a_uv;
in flat int a_texture_index;

layout (location = 0) out vec4 out_color;

uniform sampler2D u_textures[gl_MaxTextureImageUnits];

void main()
{
    int texture_index = -1;

    switch (a_texture_index)
    {
        case 0:  texture_index = 0;  break;
        case 1:  texture_index = 1;  break;
        case 2:  texture_index = 2;  break;
        case 3:  texture_index = 3;  break;
        case 4:  texture_index = 4;  break;
        case 5:  texture_index = 5;  break;
        case 6:  texture_index = 6;  break;
        case 7:  texture_index = 7;  break;
        case 8:  texture_index = 8;  break;
        case 9:  texture_index = 9;  break;
        case 10: texture_index = 10; break;
        case 11: texture_index = 11; break;
        case 12: texture_index = 12; break;
        case 13: texture_index = 13; break;
        case 14: texture_index = 14; break;
        case 15: texture_index = 15; break;
        case 16: texture_index = 16; break;
        case 17: texture_index = 17; break;
        case 18: texture_index = 18; break;
        case 19: texture_index = 19; break;
        case 20: texture_index = 20; break;
        case 21: texture_index = 21; break;
        case 22: texture_index = 22; break;
        case 23: texture_index = 23; break;
        case 24: texture_index = 24; break;
        case 25: texture_index = 25; break;
        case 26: texture_index = 26; break;
        case 27: texture_index = 27; break;
        case 28: texture_index = 28; break;
        case 29: texture_index = 29; break;
        case 30: texture_index = 30; break;
        case 31: texture_index = 31; break;
    };

    out_color = texture(u_textures[texture_index], a_uv) * a_color;
}