#vertex

#version 430 core

layout (location = 0) in uint in_data0;
layout (location = 1) in uint in_data1;

out vec2 a_uv;
out flat uint a_data0;
out vec4 a_biome_color;

uniform mat4 u_view;
uniform mat4 u_projection;
uniform vec3 u_chunk_position;

#define BLOCK_X_MASK 15
#define BLOCK_Y_MASK 255
#define BLOCK_Z_MASK 15
#define LOCAL_POSITION_ID_MASK 7
#define FACE_ID_MASK 7
#define FACE_CORNER_ID_MASK 3

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

    vec3 position = u_chunk_position + block_coords + local_positions[local_position_id] + vec3(0.5f, 0.5f, 0.5f);
    gl_Position = u_projection * u_view * vec4(position, 1.0f);

    int uv_index = int(in_data1);
    float u = texelFetch(u_uvs, uv_index).r;
    float v = texelFetch(u_uvs, uv_index + 1).r;
    a_uv = vec2(u, v);

    a_data0 = in_data0;
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
in vec4 a_biome_color;

uniform sampler2D u_block_sprite_sheet;

vec3 light_dir = normalize(vec3(0.0f, 1.0f, 0.0f));
vec3 light_color = vec3(1.0f, 1.0f, 1.0f);

#define BLOCK_X_MASK 15
#define BLOCK_Y_MASK 255
#define BLOCK_Z_MASK 15
#define LOCAL_POSITION_ID_MASK 7
#define FACE_ID_MASK 7
#define FACE_CORNER_ID_MASK 3

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
    // uint face_corner_id = (a_data0 >> 22) & FACE_CORNER_ID_MASK;
    // uint flags = a_data0 >> 24;

    vec3 normal = face_normal[face_id];

    vec4 color = texture(u_block_sprite_sheet, a_uv) * a_biome_color;

    float ambient_amount = 0.2f;
    vec3 ambient = light_color * ambient_amount;

    float diffuse_amount = max(dot(normal, light_dir), 0.0f);
    vec3 diffuse = light_color * diffuse_amount;

    out_color = vec4(ambient + diffuse, 1.0f) * color;
    // out_color = color;
}