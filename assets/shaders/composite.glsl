#vertex

#version 430 core

layout (location = 0) in vec3 in_position;
layout (location = 0) in vec2 in_uv;

out vec2 a_uv;

void main()
{
    gl_Position = vec4(in_position, 1.0f);
    a_uv = in_uv;
}

#fragment

#version 430 core

layout (location = 0) out vec4 color;

in vec2 a_uv;

uniform sampler2D u_accum;
uniform sampler2D u_reveal;

const float Epsilon = 0.00001f;

bool is_approximately_equal(float a, float b)
{
    return abs(a - b) <= (abs(a) < abs(b) ? abs(b) : abs(a)) * Epsilon;
}

float get_max(vec3 v)
{
    return max(v.x, max(v.y, v.z));
}

void main()
{
    ivec2 coords = ivec2(gl_FragCoord.xy);

    float revealage = texelFetch(u_reveal, coords, 0).r;

    if (is_approximately_equal(revealage, 1.0f)) discard;

    vec4 accumulation = texelFetch(u_accum, coords, 0);

    if (isinf(get_max(abs(accumulation.rgb))))
        accumulation.rgb = vec3(accumulation.a);

    vec3 average_color = accumulation.rgb / max(accumulation.a, Epsilon);
    color = vec4(average_color, 1.0f - revealage);
}