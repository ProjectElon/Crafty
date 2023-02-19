#vertex

#version 430 core

const vec2 positions[6] = const vec2[](

    const vec2(-1.0f,  1.0f),
    const vec2( 1.0f, -1.0f),
    const vec2(-1.0f, -1.0f),

    const vec2(-1.0f,  1.0f),
    const vec2( 1.0f,  1.0f),
    const vec2( 1.0f, -1.0f)
);

void main()
{
    gl_Position = vec4(positions[gl_VertexID], 0.0f, 1.0f);
}

#fragment

#version 430 core

layout (location = 0) out vec4 out_color;

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
    out_color = vec4(average_color, 1.0f - revealage);
}