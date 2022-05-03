#vertex

#version 430 core

layout (location = 0) in uint in_data0;
layout (location = 1) in uint in_data1;

layout (location = 2) in ivec2 instance_chunk_coords;

out vec2 a_uv;
out flat uint a_data0;
out flat uint a_data1;

out vec4 a_biome_color;
out float a_fog_factor;

uniform mat4  u_view;
uniform mat4  u_projection;
uniform float u_one_over_chunk_radius;
uniform vec3  u_camera_position;

#define BLOCK_X_MASK 15
#define BLOCK_Y_MASK 255
#define BLOCK_Z_MASK 15
#define LOCAL_POSITION_ID_MASK 7
#define FACE_ID_MASK 7
#define FACE_CORNER_ID_MASK 3
#define LIGHT_LEVEL_MASK 15

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
uniform samplerBuffer u_uvs;

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
    // uint face_corner_id = (in_data0 >> 22) & FACE_CORNER_ID_MASK;
    uint flags = in_data0 >> 24;

    vec3 position = vec3(instance_chunk_coords.x * CHUNK_WIDTH, 0.0f, instance_chunk_coords.y * CHUNK_DEPTH) + block_coords + local_positions[local_position_id] + vec3(0.5f, 0.5f, 0.5f);
    gl_Position = u_projection * u_view * vec4(position, 1.0f);

    float distance_relative_to_camera = length(u_camera_position - position);
    a_fog_factor = clamp(distance_relative_to_camera * u_one_over_chunk_radius, 0.0f, 1.0f);

    int uv_index = int(in_data1 >> 4);
    float u = texelFetch(u_uvs, uv_index).r;
    float v = texelFetch(u_uvs, uv_index + 1).r;
    a_uv = vec2(u, v);

    a_data0 = in_data0;
    a_data1 = in_data1;
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
}

#fragment

#version 430 core

layout (location = 0) out vec4 out_color;

in vec2 a_uv;
in flat uint a_data0;
in flat uint a_data1;
in vec4 a_biome_color;
in float a_fog_factor;

uniform vec4 u_sky_color;
uniform sampler2D u_block_sprite_sheet;

vec3 light_dir = normalize(vec3(0.0f, 1.0f, 0.0f));
vec3 light_color = vec3(1.0f, 1.0f, 1.0f);

#define BLOCK_X_MASK 15
#define BLOCK_Y_MASK 255
#define BLOCK_Z_MASK 15
#define LOCAL_POSITION_ID_MASK 7
#define FACE_ID_MASK 7
#define FACE_CORNER_ID_MASK 3
#define LIGHT_LEVEL_MASK 15

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
    uint face_id = (a_data0 >> 19) & FACE_ID_MASK;
    uint light_level = a_data1 & LIGHT_LEVEL_MASK;
    float light_level_factor = light_level / 15.0f;

    // uint face_corner_id = (a_data0 >> 22) & FACE_CORNER_ID_MASK;
    // uint flags = a_data0 >> 24;

    vec3 normal = face_normal[face_id];
    vec4 color = texture(u_block_sprite_sheet, a_uv) * a_biome_color;

    // phong
    // float ambient_amount = 0.4f;
    // vec3 ambient = light_color * ambient_amount;

    // float diffuse_amount = max(dot(normal, light_dir), 0.0f);
    // vec3 diffuse = light_color * diffuse_amount;
    // out_color = vec4(ambient + diffuse, 1.0f) * color;
    // out_color = mix(out_color, u_sky_color, a_fog_factor);

    out_color = mix(color, u_sky_color, a_fog_factor) * light_level_factor;
}