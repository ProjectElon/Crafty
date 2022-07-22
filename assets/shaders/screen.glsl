#vertex

#version 430 core

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec2 in_uv;

out vec2 a_uv;

void main()
{
    gl_Position = vec4(in_position, 1.0f);
    a_uv = in_uv;
}

#fragment

#version 430 core

in vec2 a_uv;

layout (location = 0) out vec4 color;

uniform sampler2D u_screen;

void main()
{
    color = vec4(texture(u_screen, a_uv).rgb, 1.0f);
}