#vertex

#version 440 core

layout (location = 0) in vec3 in_position;

void main()
{
    gl_Position = vec4(in_position, 1.0f);
}

#fragment

#version 440 core

layout (location = 0) out vec4 out_color;

void main()
{
    out_color = vec4(1.0f, 0.0f, 0.0f, 1.0f);
}