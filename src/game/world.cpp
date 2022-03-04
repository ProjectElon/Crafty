#include "world.h"
#include <glm/gtc/noise.hpp>

namespace minecraft {

    bool Chunk::initialize(const glm::ivec2& world_coords)
    {
        this->world_coords = world_coords;
        this->position = { world_coords.x * MC_CHUNK_WIDTH, 0.0f, world_coords.y * MC_CHUNK_DEPTH };
        this->render_data.vertex_count = 0;
        this->render_data.face_count = 0;
        this->render_data.vertex_array_id = 0;
        this->render_data.vertex_buffer_id = 0;
        this->loaded = false;
        this->ready_for_rendering = false;
        return true;
    }

    inline static glm::vec2 get_sample(i32 seed, const glm::ivec2& chunk_coords, const glm::ivec2& block_xz_coords)
    {
        return {
            seed + chunk_coords.x * MC_CHUNK_WIDTH + block_xz_coords.x + 0.5f,
            seed + chunk_coords.y * MC_CHUNK_DEPTH + block_xz_coords.y + 0.5f };
    }

    inline static f32 get_noise01(const glm::vec2& sample, f32 one_over_noise_scale)
    {
        f32 noise = glm::simplex(sample * one_over_noise_scale);
        return (noise + 1.0f) / 2.0f;
    }

    inline static f32 get_height_from_noise01(i32 min_height, i32 max_height, f32 noise)
    {
        return glm::trunc(min_height + ((max_height - min_height) * noise));
    }

    inline static void set_block_id_based_on_height(Block *block, i32 block_y, u32 height)
    {
        if (block_y > height)
        {
            block->id = BlockId_Air;
        }
        else if (block_y == height)
        {
            block->id = BlockId_Grass;
        }
        else
        {
            block->id = BlockId_Dirt;
        }
    }

    void Chunk::generate(i32 seed)
    {
        i32 min_biome_height = 150;
        i32 max_biome_height = 240;

        const f32 noise_scale = 256.0f;
        const f32 one_over_nosie_scale = 1.0f / noise_scale;

        const glm::ivec2 front_chunk_coords = { world_coords.x + 0, world_coords.y - 1 };
        const glm::ivec2 back_chunk_coords  = { world_coords.x + 0, world_coords.y + 1 };
        const glm::ivec2 left_chunk_coords  = { world_coords.x - 1, world_coords.y + 0 };
        const glm::ivec2 right_chunk_coords = { world_coords.x + 1, world_coords.y + 0 };

        for (i32 z = 0; z < MC_CHUNK_DEPTH; ++z)
        {
            for (i32 x = 0; x < MC_CHUNK_WIDTH; ++x)
            {
                glm::ivec2 block_xz_coords = { x, z };
                glm::vec2 sample = get_sample(seed, world_coords, block_xz_coords);
                f32 noise = get_noise01(sample, one_over_nosie_scale);
                height_map[z][x] = get_height_from_noise01(min_biome_height, max_biome_height, noise);
            }
        }

        for (i32 x = 0; x < MC_CHUNK_WIDTH; ++x)
        {
            glm::ivec2 block_xz_coords = { x, MC_CHUNK_DEPTH - 1 };
            glm::vec2 sample = get_sample(seed, front_chunk_coords, block_xz_coords);
            f32 noise = get_noise01(sample, one_over_nosie_scale);
            top_edge_height_map[x] = get_height_from_noise01(min_biome_height, max_biome_height, noise);
        }

        for (i32 x = 0; x < MC_CHUNK_WIDTH; ++x)
        {
            glm::ivec2 block_xz_coords = { x, 0 };
            glm::vec2 sample = get_sample(seed, back_chunk_coords, block_xz_coords);
            f32 noise = get_noise01(sample, one_over_nosie_scale);
            bottom_edge_height_map[x] = get_height_from_noise01(min_biome_height, max_biome_height, noise);
        }

        for (i32 z = 0; z < MC_CHUNK_DEPTH; ++z)
        {
            glm::ivec2 block_xz_coords = { MC_CHUNK_WIDTH - 1, z };
            glm::vec2 sample = get_sample(seed, left_chunk_coords, block_xz_coords);
            f32 noise = get_noise01(sample, one_over_nosie_scale);
            left_edge_height_map[z] = get_height_from_noise01(min_biome_height, max_biome_height, noise);
        }

        for (i32 z = 0; z < MC_CHUNK_DEPTH; ++z)
        {
            glm::ivec2 block_xz_coords = { 0, z };
            glm::vec2 sample = get_sample(seed, right_chunk_coords, block_xz_coords);
            f32 noise = get_noise01(sample, one_over_nosie_scale);
            right_edge_height_map[z] = get_height_from_noise01(min_biome_height, max_biome_height, noise);
        }

        for (i32 y = 0; y < MC_CHUNK_HEIGHT; ++y)
        {
            for (i32 z = 0; z < MC_CHUNK_DEPTH; ++z)
            {
                for (i32 x = 0; x < MC_CHUNK_WIDTH; ++x)
                {
                    glm::ivec3 block_coords = { x, y, z };
                    Block *block = this->get_block(block_coords);
                    const i32& height = height_map[z][x];
                    set_block_id_based_on_height(block, y, height);
                }
            }
        }

        for (i32 y = 0; y < MC_CHUNK_HEIGHT; ++y)
        {
            for (i32 x = 0; x < MC_CHUNK_WIDTH; ++x)
            {
                const i32& top_edge_height = top_edge_height_map[x];
                Block *top_edge_block = &top_edge_blocks[y * MC_CHUNK_WIDTH + x];
                set_block_id_based_on_height(top_edge_block, y, top_edge_height);

                const i32& bottom_edge_height = bottom_edge_height_map[x];
                Block *bottom_edge_block = &bottom_edge_blocks[y * MC_CHUNK_WIDTH + x];
                set_block_id_based_on_height(bottom_edge_block, y, bottom_edge_height);
            }

            for (i32 z = 0; z < MC_CHUNK_DEPTH; ++z)
            {
                const i32& left_edge_height = left_edge_height_map[z];
                Block *left_edge_block = &left_edge_blocks[y * MC_CHUNK_DEPTH + z];
                set_block_id_based_on_height(left_edge_block, y, left_edge_height);

                const i32& right_edge_height = right_edge_height_map[z];
                Block *right_edge_block = &right_edge_blocks[y * MC_CHUNK_DEPTH + z];
                set_block_id_based_on_height(right_edge_block, y, right_edge_height);
            }
        }
    }

    Block* Chunk::get_block(const glm::ivec3& block_coords)
    {
        assert(block_coords.x >= 0 && block_coords.x < MC_CHUNK_WIDTH &&
               block_coords.y >= 0 && block_coords.y < MC_CHUNK_HEIGHT &&
               block_coords.z >= 0 && block_coords.z < MC_CHUNK_DEPTH);
        i32 block_index = get_block_index(block_coords);
        return this->blocks + block_index;
    }

    Block* Chunk::get_neighbour_block_from_right(const glm::ivec3& block_coords)
    {
        if (block_coords.x == MC_CHUNK_WIDTH - 1)
        {
            return &World::null_block;
        }

        return this->get_block({ block_coords.x + 1, block_coords.y, block_coords.z });
    }

    Block* Chunk::get_neighbour_block_from_left(const glm::ivec3& block_coords)
    {
        if (block_coords.x == 0)
        {
            return &World::null_block;
        }

        return this->get_block({ block_coords.x - 1, block_coords.y, block_coords.z });
    }

    Block* Chunk::get_neighbour_block_from_top(const glm::ivec3& block_coords)
    {
        if (block_coords.y == MC_CHUNK_HEIGHT - 1)
        {
            return &World::null_block;
        }

        return this->get_block({ block_coords.x, block_coords.y + 1, block_coords.z });
    }

    Block* Chunk::get_neighbour_block_from_bottom(const glm::ivec3& block_coords)
    {
        if (block_coords.y == 0)
        {
            return &World::null_block;
        }
        return this->get_block({ block_coords.x, block_coords.y - 1, block_coords.z });
    }

    Block* Chunk::get_neighbour_block_from_front(const glm::ivec3& block_coords)
    {
        if (block_coords.z == 0)
        {
            return &World::null_block;
        }
        return this->get_block({ block_coords.x, block_coords.y, block_coords.z - 1 });
    }

    Block* Chunk::get_neighbour_block_from_back(const glm::ivec3& block_coords)
    {
        if (block_coords.z == MC_CHUNK_DEPTH - 1)
        {
            return &World::null_block;
        }

        return this->get_block({ block_coords.x, block_coords.y, block_coords.z + 1 });
    }


    const Block_Info World::block_infos[BlockId_Count] =
    {
        // Air
        {
            0,
            0,
            0,
            BlockFlags_Is_Transparent
        },
        // Grass
        {
            Texture_Id_grass_block_top,
            Texture_Id_dirt,
            Texture_Id_grass_block_side,
            BlockFlags_Is_Solid | BlockFlags_Should_Color_Top_By_Biome
        },
        // Sand
        {
            Texture_Id_sand,
            Texture_Id_sand,
            Texture_Id_sand,
            BlockFlags_Is_Solid
        },
        // Dirt
        {
            Texture_Id_dirt,
            Texture_Id_dirt,
            Texture_Id_dirt,
            BlockFlags_Is_Solid
        },
        // Stone
        {
            Texture_Id_stone,
            Texture_Id_stone,
            Texture_Id_stone,
            BlockFlags_Is_Solid
        },
        // Green Concrete
        {
            Texture_Id_green_concrete_powder,
            Texture_Id_green_concrete_powder,
            Texture_Id_green_concrete_powder,
            BlockFlags_Is_Solid
        },
        // BedRock
        {
            Texture_Id_bedrock,
            Texture_Id_bedrock,
            Texture_Id_bedrock,
            BlockFlags_Is_Solid,
        },
        // Oak Log
        {
            Texture_Id_oak_log_top,
            Texture_Id_oak_log_top,
            Texture_Id_oak_log,
            BlockFlags_Is_Solid,
        },
        // Oak Leaves
        {
            Texture_Id_oak_leaves,
            Texture_Id_oak_leaves,
            Texture_Id_oak_leaves,
            BlockFlags_Is_Solid |
            BlockFlags_Is_Transparent |
            BlockFlags_Should_Color_Top_By_Biome |
            BlockFlags_Should_Color_Side_By_Biome |
            BlockFlags_Should_Color_Bottom_By_Biome
        },
        // Oak Planks
        {
            Texture_Id_oak_planks,
            Texture_Id_oak_planks,
            Texture_Id_oak_planks,
            BlockFlags_Is_Solid
        },
        // Glow Stone
        {
            Texture_Id_glowstone,
            Texture_Id_glowstone,
            Texture_Id_glowstone,
            BlockFlags_Is_Solid
        },
        // Cobble Stone
        {
            Texture_Id_cobblestone,
            Texture_Id_cobblestone,
            Texture_Id_cobblestone,
            BlockFlags_Is_Solid
        },
        // Spruce Log
        {
            Texture_Id_spruce_log_top,
            Texture_Id_spruce_log_top,
            Texture_Id_spruce_log,
            BlockFlags_Is_Solid
        },
        // Spruce Planks
        {
            Texture_Id_spruce_planks,
            Texture_Id_spruce_planks,
            Texture_Id_spruce_planks,
            BlockFlags_Is_Solid
        },
        // Glass
        {
            Texture_Id_glass,
            Texture_Id_glass,
            Texture_Id_glass,
            BlockFlags_Is_Solid | BlockFlags_Is_Transparent
        },
        // Sea Lantern
        {
            Texture_Id_sea_lantern,
            Texture_Id_sea_lantern,
            Texture_Id_sea_lantern,
            BlockFlags_Is_Solid
        },
        // Birch Log
        {
            Texture_Id_birch_log_top,
            Texture_Id_birch_log_top,
            Texture_Id_birch_log,
            BlockFlags_Is_Solid
        },
        // Blue Stained Glass
        {
            Texture_Id_blue_stained_glass,
            Texture_Id_blue_stained_glass,
            Texture_Id_blue_stained_glass,
            BlockFlags_Is_Solid | BlockFlags_Is_Transparent,
        },
        // Water
        {
            Texture_Id_water,
            Texture_Id_water,
            Texture_Id_water,
            BlockFlags_Is_Transparent,
        },
        // Birch Planks
        {
            Texture_Id_birch_planks,
            Texture_Id_birch_planks,
            Texture_Id_birch_planks,
            BlockFlags_Is_Solid
        },
        // Diamond
        {
            Texture_Id_diamond_block,
            Texture_Id_diamond_block,
            Texture_Id_diamond_block,
            BlockFlags_Is_Solid
        },
        // Obsidian
        {
            Texture_Id_obsidian,
            Texture_Id_obsidian,
            Texture_Id_obsidian,
            BlockFlags_Is_Solid
        },
        // Crying Obsidian
        {
            Texture_Id_crying_obsidian,
            Texture_Id_crying_obsidian,
            Texture_Id_crying_obsidian,
            BlockFlags_Is_Solid
        },
        // Dark Oak Log
        {
            Texture_Id_dark_oak_log_top,
            Texture_Id_dark_oak_log_top,
            Texture_Id_dark_oak_log,
            BlockFlags_Is_Solid
        },
        // Dark Oak Planks
        {
            Texture_Id_dark_oak_planks,
            Texture_Id_dark_oak_planks,
            Texture_Id_dark_oak_planks,
            BlockFlags_Is_Solid
        },
        // Jungle Log
        {
            Texture_Id_jungle_log_top,
            Texture_Id_jungle_log_top,
            Texture_Id_jungle_log,
            BlockFlags_Is_Solid
        },
        // Jungle Planks
        {
            Texture_Id_jungle_planks,
            Texture_Id_jungle_planks,
            Texture_Id_jungle_planks,
            BlockFlags_Is_Solid
        },
        // Acacia Log
        {
            Texture_Id_acacia_log_top,
            Texture_Id_acacia_log_top,
            Texture_Id_acacia_log,
            BlockFlags_Is_Solid
        },
        // Acacia Planks
        {
            Texture_Id_acacia_planks,
            Texture_Id_acacia_planks,
            Texture_Id_acacia_planks,
            BlockFlags_Is_Solid
        }
    };

    Block World::null_block = { BlockId_Air };
    std::unordered_map< glm::ivec2, Chunk*, Chunk_Hash > World::loaded_chunks;
}