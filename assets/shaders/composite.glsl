#vertex

#version 430 core

layout (location = 0) in vec3 in_position;

void main()
{
    gl_Position = vec4(in_position, 1.0f);
}

#fragment

#version 430 core

layout (location = 0) out vec4 out_color;

uniform int u_samples;
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
    // ivec2 coords = ivec2(gl_FragCoord.xy);

    // vec3 color_accum = vec3(0.0f);
    // float revealage_accum = 0.0f;

    // for (int texel_index = 0; texel_index < u_samples; texel_index++)
    // {
    //     float revealage = texelFetch(u_reveal, coords, texel_index).r;
    //     revealage_accum += revealage;

    //     vec4 accumulation = texelFetch(u_accum, coords, texel_index);

    //     if (isinf(get_max(abs(accumulation.rgb)))) accumulation.rgb = vec3(accumulation.a);

    //     color_accum += accumulation.rgb / max(accumulation.a, Epsilon);
    // }

    // // float revealage = revealage_accum / float(u_samples);
    // float revealage = revealage_accum;

    // if (is_approximately_equal(revealage, 1.0f)) discard;
    // // vec3 color = color_accum / float(u_samples);
    // vec3 color = color_accum;
    // out_color = vec4(color, 1.0f - revealage);

    ivec2 coords = ivec2(gl_FragCoord.xy);

    float revealage = texelFetch(u_reveal, coords, 0).r;

    if (is_approximately_equal(revealage, 1.0f)) discard;

    vec4 accumulation = texelFetch(u_accum, coords, 0);

    if (isinf(get_max(abs(accumulation.rgb))))
        accumulation.rgb = vec3(accumulation.a);

    vec3 average_color = accumulation.rgb / max(accumulation.a, Epsilon);
    out_color = vec4(average_color, 1.0f - revealage);
}