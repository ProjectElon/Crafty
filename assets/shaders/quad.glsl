#vertex

#version 430 core

layout (location = 0) in vec2 in_position;

// per instance
layout (location = 1) in vec2 instance_position;
layout (location = 2) in vec2 instance_scale;
layout (location = 3) in vec4 instance_color;

out vec4 a_color;

uniform mat4 u_view;
uniform mat4 u_projection;

void main()
{
    gl_Position = u_projection * u_view * vec4(in_position * instance_scale + instance_position, 0.0f, 1.0f);
    a_color = instance_color;
}

#fragment

#version 430 core

in vec4 a_color;

layout (location = 0) out vec4 out_color;

void main()
{
    out_color = a_color;
}