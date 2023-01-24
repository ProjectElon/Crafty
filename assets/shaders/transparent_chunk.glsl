#vertex

#version 450 core

layout (location = 0) in uint  in_data0;
layout (location = 1) in uint  in_data1;
layout (location = 2) in ivec2 in_instance_chunk_coords;

out vec2 a_uv;
out flat int a_texture_id;

vec2 uv_look_up[4] = vec2[](
    vec2(1.0f, 0.0f),
    vec2(0.0f, 0.0f),
    vec2(0.0f, 1.0f),
    vec2(1.0f, 1.0f)
);

out vec4 a_biome_color;
out flat vec4 a_highlight_color;
out float a_fog_factor;
out float a_light_level;

uniform mat4  u_view;
uniform mat4  u_projection;
uniform float u_one_over_chunk_radius;
uniform vec3  u_camera_position;
uniform float u_sky_light_level;

uniform ivec3 u_highlighted_block_coords;
uniform ivec2 u_highlighted_block_chunk_coords;

#define BLOCK_X_MASK 15
#define BLOCK_Y_MASK 255
#define BLOCK_Z_MASK 15
#define LOCAL_POSITION_ID_MASK 7
#define FACE_ID_MASK 7
#define FACE_CORNER_ID_MASK 3
#define SKY_LIGHT_LEVEL_MASK 15
#define LIGHT_SOURCE_LEVEL_MASK 15
#define AMBIENT_OCCLUSION_LEVEL_MASK 3

vec3 local_positions[8] = vec3[](
    vec3( 0.5f,  0.5f,  0.5f), // 0
    vec3(-0.5f,  0.5f,  0.5f), // 1
    vec3(-0.5f,  0.5f, -0.5f), // 2
    vec3( 0.5f,  0.5f, -0.5f), // 3
    vec3( 0.5f, -0.5f,  0.5f), // 4
    vec3(-0.5f, -0.5f,  0.5f), // 5
    vec3(-0.5f, -0.5f, -0.5f), // 6
    vec3( 0.5f, -0.5f, -0.5f)  // 7
);

uniform vec4 u_biome_color;
uniform sampler2DArray u_block_array_texture;

#define Top_Face_ID    0
#define Bottom_Face_ID 1
#define Left_Face_ID   2
#define Right_Face_ID  3
#define Front_Face_ID  4
#define Back_Face_ID   5

#define BlockFlags_Is_Solid 1
#define BlockFlags_Is_Transparent 2
#define BlockFlags_Should_Color_Top_By_Biome 4
#define BlockFlags_Should_Color_Side_By_Biome 8
#define BlockFlags_Should_Color_Bottom_By_Biome 16
#define BlockFlags_Is_Light_Source 32

#define CHUNK_WIDTH 16
#define CHUNK_DEPTH 16

void main()
{
    uint block_x = in_data0 & BLOCK_X_MASK;
    uint block_y = (in_data0 >> 4) & BLOCK_Y_MASK;
    uint block_z = (in_data0 >> 12) & BLOCK_Z_MASK;
    vec3 block_coords = vec3(block_x, block_y, block_z);
    uint local_position_id = (in_data0 >> 16) & LOCAL_POSITION_ID_MASK;
    uint face_id = (in_data0 >> 19) & FACE_ID_MASK;
    uint face_corner_id = (in_data0 >> 22) & FACE_CORNER_ID_MASK;
    uint flags = in_data0 >> 24;

    vec3 position = vec3(in_instance_chunk_coords.x * CHUNK_WIDTH, 0.0f, in_instance_chunk_coords.y * CHUNK_DEPTH) + block_coords + local_positions[local_position_id] + vec3(0.5f, 0.5f, 0.5f);
    if ((flags & BlockFlags_Is_Solid) == 0)
    {
        position.y -= 0.05f;
    }
    gl_Position = u_projection * u_view * vec4(position, 1.0f);

    float distance_relative_to_camera = length(u_camera_position - position);
    a_fog_factor = clamp(distance_relative_to_camera * u_one_over_chunk_radius, 0.0f, 1.0f);

    a_uv         = uv_look_up[face_corner_id];
    a_texture_id = int(in_data1 >> 10);

    float sky_light_level = float(in_data1 & SKY_LIGHT_LEVEL_MASK);
    float sky_light_factor = u_sky_light_level - 15.0f;
    float sky_light = max(sky_light_level + sky_light_factor, 1.0f);
    float light_source = float((in_data1 >> 4) & LIGHT_SOURCE_LEVEL_MASK);

    a_light_level = float(max(sky_light, light_source)) / 15.0f;
    float ambient_factor = 0.5f + a_light_level * 1.5f;
    float ambient_occlusion = (float((in_data1 >> 8) & AMBIENT_OCCLUSION_LEVEL_MASK) + ambient_factor) / (3.0f + ambient_factor);

    if ((flags & BlockFlags_Is_Light_Source) == 0)
    {
        a_light_level *= ambient_occlusion;
    }

    a_biome_color = vec4(1.0f, 1.0f, 1.0f, 1.0f);

    switch (face_id)
    {
        case Top_Face_ID:
        {
            if ((flags & BlockFlags_Should_Color_Top_By_Biome) != 0)
            {
                a_biome_color = u_biome_color;
            }
        } break;

        case Left_Face_ID:
        case Right_Face_ID:
        case Front_Face_ID:
        case Back_Face_ID:
        {
            if ((flags & BlockFlags_Should_Color_Side_By_Biome) != 0)
            {
                a_biome_color = u_biome_color;
            }
        } break;

        case Bottom_Face_ID:
        {
            if ((flags & BlockFlags_Should_Color_Bottom_By_Biome) != 0)
            {
                a_biome_color = u_biome_color;
            }
        } break;
    }

    a_highlight_color = vec4(1.0f, 1.0f, 1.0f, 1.0f);

    if (u_highlighted_block_coords.x == block_x &&
        u_highlighted_block_coords.y == block_y &&
        u_highlighted_block_coords.z == block_z &&
        u_highlighted_block_chunk_coords.x == in_instance_chunk_coords.x &&
        u_highlighted_block_chunk_coords.y == in_instance_chunk_coords.y)
    {
        a_highlight_color = vec4(0.5f, 0.5f, 0.5f, 1.0f);
    }
}

#fragment

#version 450 core

layout (location = 0) out vec4  accum;
layout (location = 1) out float reveal;

in vec2 a_uv;
in flat int a_texture_id;

in vec4 a_biome_color;
in flat vec4 a_highlight_color;
in float a_fog_factor;
in float a_light_level;

uniform vec4 u_sky_color;
uniform vec4 u_tint_color;

uniform sampler2DArray u_block_array_texture;

vec3 light_dir   = normalize(vec3(0.0f, 1.0f, 0.0f));
vec3 light_color = vec3(1.0f, 1.0f, 1.0f);

#define BLOCK_X_MASK 15
#define BLOCK_Y_MASK 255
#define BLOCK_Z_MASK 15
#define LOCAL_POSITION_ID_MASK 7
#define FACE_ID_MASK 7
#define FACE_CORNER_ID_MASK 3

#define Top_Face_ID 0
#define Bottom_Face_ID 1
#define Left_Face_ID 2
#define Right_Face_ID 3
#define Front_Face_ID 4
#define Back_Face_ID 5

#define BlockFlags_Is_Solid 1
#define BlockFlags_Is_Transparent 2
#define BlockFlags_Should_Color_Top_By_Biome 4
#define BlockFlags_Should_Color_Side_By_Biome 8
#define BlockFlags_Should_Color_Bottom_By_Biome 16

vec3 face_normal[6] = vec3[](
    vec3( 0.0f,  1.0f,  0.0f), // top
    vec3( 0.0f, -1.0f,  0.0f), // bottom
    vec3(-1.0f,  0.0f,  0.0f), // left
    vec3( 1.0f,  0.0f,  0.0f), // right
    vec3( 0.0f,  0.0f, -1.0f), // font
    vec3( 0.0f,  0.0f,  1.0f)  // back
);

void main()
{
    // uint face_id = (a_data0 >> 19) & FACE_ID_MASK;

    // uint face_corner_id = (a_data0 >> 22) & FACE_CORNER_ID_MASK;
    // uint flags = a_data0 >> 24;

    // vec3 normal = face_normal[face_id];

    vec4 block_color = texture(u_block_array_texture, vec3(a_uv.x, a_uv.y, a_texture_id)) * a_biome_color * a_highlight_color;
    vec4 color = mix(vec4(block_color.rgb * a_light_level, block_color.a), u_sky_color, a_fog_factor) * u_tint_color;

    float weight = clamp(pow(min(1.0, color.a * 10.0) + 0.01, 3.0) * 1e8 * pow(1.0 - gl_FragCoord.z * 0.9, 3.0), 1e-2, 3e3);

    accum = vec4(color.rgb * color.a, color.a) * weight;
    reveal = color.a;
}