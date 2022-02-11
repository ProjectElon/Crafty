#vertex

#version 430 core

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec2 in_uv;
layout (location = 2) in uint in_block_flags;

out vec2 a_uv;
out flat uint a_block_flags;

uniform mat4 u_view;
uniform mat4 u_projection;

void main()
{
    gl_Position = u_projection * u_view * vec4(in_position, 1.0f);
    a_uv = in_uv;
    a_block_flags = in_block_flags;
}

#fragment

#version 430 core

layout (location = 0) out vec4 out_color;

in vec2 a_uv;
in flat uint a_block_flags;

uniform sampler2D u_block_sprite_sheet;
uniform vec4 u_biome_color;

vec3 light_dir = normalize(vec3(-0.27f, 0.57f, -0.57f));
vec3 light_color = vec3(1.0f, 1.0f, 1.0f);

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
    vec3( 0.0f, -1.0f,  0.0f), // bottom,
    vec3(-1.0f,  0.0f,  0.0f), // left
    vec3( 1.0f,  0.0f,  0.0f), // right
    vec3( 0.0f,  0.0f, -1.0f), // font
    vec3( 0.0f,  0.0f,  1.0f)  // back
);

void main()
{
    uint flags = a_block_flags;
    uint face = (a_block_flags >> 4);

    vec3 normal = face_normal[face];
    
    float ambient_amount = 0.4f;
    vec3 ambient = ambient_amount * light_color;

    float diffuse_amount = max(dot(normal, light_dir), 0.0f);
    vec3 diffuse = diffuse_amount * light_color;

    vec4 biome_color = vec4(1.0f, 1.0f, 1.0f, 1.0f);  

    switch (face)
    {
        case Top_Face_ID:
        {
            if ((flags & BlockFlags_Should_Color_Top_By_Biome) != 0) 
            {
                biome_color = u_biome_color;
            }
        } break;

        case Left_Face_ID:
        case Right_Face_ID:
        case Front_Face_ID:
        case Back_Face_ID:
        {
            if ((flags & BlockFlags_Should_Color_Side_By_Biome) != 0) 
            {
                biome_color = u_biome_color;
            }
        } break;

        case Bottom_Face_ID:
        {
            if ((flags & BlockFlags_Should_Color_Bottom_By_Biome) != 0) 
            {
                biome_color = u_biome_color;
            }
        } break;
    }

    vec4 color = texture(u_block_sprite_sheet, a_uv) * biome_color;
    out_color = vec4((ambient + diffuse), 1.0f) * color;
}