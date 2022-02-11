#include "world.h"

#include <glm/gtc/noise.hpp>
#include <time.h>

namespace minecraft {

    bool Chunk::initialize(const glm::ivec2& world_coords)
    {
        this->render_data.current_vertex_pointer = this->render_data.vertices;
        this->render_data.face_count = 0;
        this->render_data.pending = true;

        glm::vec3 offset = { (MC_CHUNK_WIDTH - 1.0f) / 2.0f, (MC_CHUNK_HEIGHT - 1.0f) / 2.0f,  (MC_CHUNK_DEPTH - 1.0f) / 2.0f };
        
        this->world_coords = world_coords; 
        this->position = { world_coords.x * MC_CHUNK_WIDTH, MC_CHUNK_HEIGHT / 2.0f, world_coords.y * MC_CHUNK_DEPTH };
        this->first_block_position = this->position - offset;

        return true;
    }

    void Chunk::generate(u32 seed)
    {
        for (i32 y = 0; y < MC_CHUNK_HEIGHT; ++y)
        {
            for (i32 z = 0; z < MC_CHUNK_DEPTH; ++z)
            {
                for (i32 x = 0; x < MC_CHUNK_WIDTH; ++x)
                {
                    glm::ivec3 block_coords = { x, y, z };
                    
                    u32 block_index = this->get_block_index(block_coords);
                    glm::vec3 position = this->get_block_position(block_coords);
                    Block &block = this->blocks[block_index];

                    f32 simplexX = seed + (x + world_coords.x * 16);
                    f32 simplexY = seed + (z + world_coords.y * 16);
                    f32 height = glm::simplex(glm::vec2(simplexX / 1000.0f, simplexY / 1000.0f));
                    u32 maxHeight = (u32)(((height + 1.0f) / 2.0f) * 255.0f);

                    if (y >= maxHeight)
                    {
                        block.id = BlockId_Air;
                    }
                    else if (y == maxHeight - 1)
                    {
                        block.id = BlockId_Grass;
                    } 
                    else if (y <= 50)
                    {
                        block.id = BlockId_Stone;
                    }
                    else
                    {
                        block.id = BlockId_Dirt;
                    }
                }
            }
        }
    }


    Block* Chunk::get_neighbour_block_from_right(const glm::ivec3& block_coords)
    {
        if (block_coords.x + 1 >= MC_CHUNK_WIDTH)
        {
            return &World::null_block;   
        }

        return this->get_block({ block_coords.x + 1, block_coords.y, block_coords.z });
    }

    Block* Chunk::get_neighbour_block_from_left(const glm::ivec3& block_coords)
    {
        if (block_coords.x - 1 < 0)
        {
            return &World::null_block;
        }

        return this->get_block({ block_coords.x - 1, block_coords.y, block_coords.z });
    }

    Block* Chunk::get_neighbour_block_from_top(const glm::ivec3& block_coords)
    {
        if (block_coords.y + 1 >= MC_CHUNK_HEIGHT)
        {
            return &World::null_block;
        }

        return this->get_block({ block_coords.x, block_coords.y + 1, block_coords.z });
    }

    Block* Chunk::get_neighbour_block_from_bottom(const glm::ivec3& block_coords)
    {
        if (block_coords.y - 1 < 0)
        {
            return &World::null_block;
        }

        return this->get_block({ block_coords.x, block_coords.y - 1, block_coords.z });
    }

    Block* Chunk::get_neighbour_block_from_front(const glm::ivec3& block_coords)
    {
        if (block_coords.z - 1 < 0)
        {
            return &World::null_block;
        }
        return this->get_block({ block_coords.x, block_coords.y, block_coords.z - 1 });
    }

    Block* Chunk::get_neighbour_block_from_back(const glm::ivec3& block_coords)
    {
        if (block_coords.z + 1 >= MC_CHUNK_DEPTH)
        {
            return &World::null_block;
        }

        return this->get_block({ block_coords.x, block_coords.y, block_coords.z + 1 });
    }

    std::unordered_map< i64, Chunk* > World::loaded_chunks;

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
}