#vertex

#version 440 core

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

#version 440 core

precision mediump float;

in vec2 a_uv;

layout (location = 0) out vec4 out_color;

uniform vec2 u_screen_size;
uniform bool u_enable_fxaa;
uniform sampler2D u_screen;

#ifndef FXAA_REDUCE_MIN
    #define FXAA_REDUCE_MIN   (1.0f / 128.0f)
#endif
#ifndef FXAA_REDUCE_MUL
    #define FXAA_REDUCE_MUL   (1.0f / 8.0f)
#endif
#ifndef FXAA_SPAN_MAX
    #define FXAA_SPAN_MAX     8.0f
#endif

//optimized version for mobile, where dependent
//texture reads can be a bottleneck
vec4 fxaa(sampler2D tex, vec2 fragCoord, vec2 resolution,
            vec2 v_rgbNW, vec2 v_rgbNE,
            vec2 v_rgbSW, vec2 v_rgbSE,
            vec2 v_rgbM) {
    vec4 color;
    mediump vec2 inverseVP = vec2(1.0f / resolution.x, 1.0f / resolution.y);
    vec3 rgbNW = texture2D(tex, v_rgbNW).xyz;
    vec3 rgbNE = texture2D(tex, v_rgbNE).xyz;
    vec3 rgbSW = texture2D(tex, v_rgbSW).xyz;
    vec3 rgbSE = texture2D(tex, v_rgbSE).xyz;
    vec4 texColor = texture2D(tex, v_rgbM);
    vec3 rgbM  = texColor.xyz;
    vec3 luma = vec3(0.299f, 0.587f, 0.114f);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM,  luma);
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    mediump vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) *
                          (0.25f * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);

    float rcpDirMin = 1.0f / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = min(vec2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),
              max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
              dir * rcpDirMin)) * inverseVP;

    vec3 rgbA = 0.5f * (
        texture2D(tex, fragCoord * inverseVP + dir * (1.0 / 3.0 - 0.5)).xyz +
        texture2D(tex, fragCoord * inverseVP + dir * (2.0 / 3.0 - 0.5)).xyz);
    vec3 rgbB = rgbA * 0.5f + 0.25f * (
        texture2D(tex, fragCoord * inverseVP + dir * -0.5f).xyz +
        texture2D(tex, fragCoord * inverseVP + dir * 0.5f).xyz);

    float lumaB = dot(rgbB, luma);
    if ((lumaB < lumaMin) || (lumaB > lumaMax))
        color = vec4(rgbA, texColor.a);
    else
        color = vec4(rgbB, texColor.a);
    return color;
}

void main()
{
    vec2 uv = vec2(gl_FragCoord.xy / u_screen_size.xy);
    // uv.y = 1.0f - uv.y;
    vec2 fragCoord = uv * u_screen_size;
    vec2 inverseVP = 1.0f / u_screen_size;

    vec2 rgbNW = (fragCoord + vec2(-1.0f, -1.0f)) * inverseVP;
    vec2 rgbNE = (fragCoord + vec2( 1.0f, -1.0f)) * inverseVP;
    vec2 rgbSW = (fragCoord + vec2(-1.0f,  1.0f)) * inverseVP;
    vec2 rgbSE = (fragCoord + vec2( 1.0f,  1.0f)) * inverseVP;
    vec2 rgbM  = vec2(fragCoord * inverseVP);

    if (u_enable_fxaa)
    {
        out_color = fxaa(u_screen, fragCoord, u_screen_size, rgbNW, rgbNE, rgbSW, rgbSE, rgbM);
    }
    else
    {
        out_color = texture(u_screen, uv);
    }
}