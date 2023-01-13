#vertex

#version 450 core

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec4 in_color;

uniform mat4 u_view;
uniform mat4 u_projection;

out vec4 a_color;

void main()
{
    gl_Position = u_projection * u_view * vec4(in_position, 1.0f);
    a_color = in_color;
}

#fragment

#version 450 core

in vec4 a_color;

layout (location = 0) out vec4 color;

void main()
{
    color = a_color;
}