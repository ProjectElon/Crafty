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

layout (location = 0) out vec4 out_color;

in vec2 a_uv;

uniform int u_samples;
uniform sampler2DMS u_accum;
uniform sampler2DMS u_reveal;

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

    vec3 color_accum = vec3(0.0f);
    float revealage_accum = 0.0f;

    for (int texel_index = 0; texel_index < u_samples; texel_index++)
    {
        float revealage = texelFetch(u_reveal, coords, texel_index).r;
        revealage_accum += revealage;

        vec4 accumulation = texelFetch(u_accum, coords, texel_index);

        if (isinf(get_max(abs(accumulation.rgb))))
            accumulation.rgb = vec3(accumulation.a);

        color_accum += accumulation.rgb / max(accumulation.a, Epsilon);
    }

    float revealage = revealage_accum / float(u_samples);
    if (is_approximately_equal(revealage, 1.0f)) discard;
    vec3 color = color_accum / float(u_samples);
    out_color = vec4(color, 1.0f - revealage);
}