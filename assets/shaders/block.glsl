#vertex

#version 430 core

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec2 in_uv;
layout (location = 2) in uint in_face;
layout (location = 3) in uint in_should_color_top_by_biome;

out vec2 a_uv;
out flat uint a_face;
out flat uint a_should_color_top_by_biome;

uniform mat4 u_view;
uniform mat4 u_projection;

void main()
{
    gl_Position = u_projection * u_view * vec4(in_position, 1.0f);
    a_uv = in_uv;
    a_face = in_face;
    a_should_color_top_by_biome = in_should_color_top_by_biome;
}

#fragment

#version 430 core

layout (location = 0) out vec4 out_color;

in vec2 a_uv;
in flat uint a_face;
in flat uint a_should_color_top_by_biome;

uniform sampler2D u_block_sprite_sheet;
uniform vec4 u_biome_color;

vec3 light_dir = normalize(vec3(-0.27f, 0.57f, -0.57f));
vec3 light_color = vec3(1.0f, 1.0f, 1.0f);

bool equal(float a, float b, float eps = 0.0001)
{
    return abs(b - a) < eps;
}

#define TOP_FACE_ID    0
#define BOTTOM_FACE_ID 1
#define LEFT_FACE_ID   2
#define RIGHT_FACE_ID  3
#define FRONT_FACE_ID  4
#define BACK_FACE_ID   5

vec3 face_normal[6] = vec3[](
    vec3(0.0f, 1.0f, 0.0f), // top
    vec3(0.0f, -1.0f, 0.0f), // bottom,
    vec3(-1.0f, 0.0f, 0.0f), // left
    vec3(1.0f, 0.0f, 0.0f), // right
    vec3(0.0f, 0.0f, -1.0f), // font
    vec3(0.0f, 0.0f, 1.0f) // back
);

void main()
{
    vec3 normal = face_normal[a_face];
    
    float ambient_amount = 0.4f;
    vec3 ambient = ambient_amount * light_color;

    float diffuse_amount = max(dot(normal, light_dir), 0.0f);
    vec3 diffuse = diffuse_amount * light_color;

    vec4 biome_color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    
    if (a_face == TOP_FACE_ID && a_should_color_top_by_biome == 1) // top_face
    {
        biome_color = u_biome_color;
    }

    vec4 color = texture(u_block_sprite_sheet, a_uv) * biome_color;
    out_color = vec4((ambient + diffuse), 1.0f) * color;
}