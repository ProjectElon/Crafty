#include "world.h"

namespace minecraft {

    bool Chunk::initialize(World *world, const glm::vec3& position)
    {
        glm::vec3 offset = { (MC_CHUNK_WIDTH - 1.0f) / 2.0f, (MC_CHUNK_HEIGHT - 1.0f) / 2.0f,  (MC_CHUNK_DEPTH - 1.0f) / 2.0f };
        glm::vec3 block_size = glm::vec3(1.0f, 1.0f, 1.0f);
        
        this->world = world;
        this->position = position;
        this->first_block_position = position - offset * block_size;

        for (i32 y = 0; y < MC_CHUNK_HEIGHT; ++y)
        {
            for (i32 z = 0; z < MC_CHUNK_DEPTH; ++z)
            {
                for (i32 x = 0; x < MC_CHUNK_WIDTH; ++x)
                {
                    glm::ivec3 block_coords = { x, y, z };

                    Block &block = this->blocks[y][z][x];
                    glm::vec3 position = get_block_position(block_coords);
                    
                    if (position.y >= 22.0f) {
                        block.id = BlockId_Grass;
                    } else if (position.y >= 0) {
                        block.id = BlockId_Dirt;
                    } else {
                        block.id = BlockId_Stone;
                    }
                }
            }
        }

        return true;
    }

    Block* Chunk::get_neighbour_block_from_right(const glm::ivec3& block_coords)
    {
        if (block_coords.x + 1 >= MC_CHUNK_WIDTH)
        {
            Chunk *right_chunk = world->get_neighbour_chunk_from_right(this->coords);
            
            if (right_chunk == nullptr) 
            {
                return &World::null_block;
            }
            
            return &(right_chunk->blocks[block_coords.y][block_coords.z][0]);
        }

        return &(this->blocks[block_coords.y][block_coords.z][block_coords.x + 1]);
    }

    Block* Chunk::get_neighbour_block_from_left(const glm::ivec3& block_coords)
    {
        if (block_coords.x - 1 < 0)
        {
            Chunk *left_chunk = world->get_neighbour_chunk_from_left(this->coords);
            
            if (left_chunk == nullptr) 
            {
                return &World::null_block;
            }

            return &(left_chunk->blocks[block_coords.y][block_coords.z][MC_CHUNK_WIDTH - 1]);
        }

        return &(this->blocks[block_coords.y][block_coords.z][block_coords.x - 1]);
    }

    Block* Chunk::get_neighbour_block_from_top(const glm::ivec3& block_coords)
    {
        if (block_coords.y + 1 >= MC_CHUNK_HEIGHT)
        {
            Chunk *top_chunk = world->get_neighbour_chunk_from_top(this->coords);
            
            if (top_chunk == nullptr) 
            {
                return &World::null_block;
            }

            return &(top_chunk->blocks[0][block_coords.z][block_coords.x]);
        }

        return &(this->blocks[block_coords.y + 1][block_coords.z][block_coords.x]);
    }

    Block* Chunk::get_neighbour_block_from_bottom(const glm::ivec3& block_coords)
    {
        if (block_coords.y - 1 < 0)
        {
            Chunk *bottom_chunk = world->get_neighbour_chunk_from_bottom(this->coords);

            if (bottom_chunk == nullptr)
            {
                return &World::null_block;
            }

            return &(bottom_chunk->blocks[MC_CHUNK_HEIGHT - 1][block_coords.z][block_coords.x]);
        }

        return &(this->blocks[block_coords.y - 1][block_coords.z][block_coords.x]);
    }

    Block* Chunk::get_neighbour_block_from_front(const glm::ivec3& block_coords)
    {
        if (block_coords.z - 1 < 0)
        {
            Chunk *front_chunk = world->get_neighbour_chunk_from_front(this->coords);
            
            if (front_chunk == nullptr) 
            {
                return &World::null_block;
            }

            return &(front_chunk->blocks[block_coords.y][MC_CHUNK_DEPTH - 1][block_coords.x]);
        }

        return &(this->blocks[block_coords.y][block_coords.z - 1][block_coords.x]);
    }

    Block* Chunk::get_neighbour_block_from_back(const glm::ivec3& block_coords)
    {
        if (block_coords.z + 1 >= MC_CHUNK_DEPTH)
        {
            Chunk* back_chunk = world->get_neighbour_chunk_from_back(this->coords);

            if (back_chunk == nullptr)
            {
                return &World::null_block;
            }

            return &(back_chunk->blocks[block_coords.y][0][block_coords.x]);
        }

        return &(this->blocks[block_coords.y][block_coords.z + 1][block_coords.x]);
    }

    bool World::initialize(const glm::vec3& position)
    {

        glm::vec3 offset = { (MC_WORLD_WIDTH - 1) / 2.0f, (MC_WORLD_HEIGHT - 1) / 2.0f, (MC_WORLD_DEPTH - 1) / 2.0f };
        glm::vec3 chunk_size = glm::vec3(MC_CHUNK_WIDTH, MC_CHUNK_HEIGHT, MC_CHUNK_DEPTH);

        this->position = position;
        this->first_chunk_position = position - offset * chunk_size;

        for (u32 y = 0; y < MC_WORLD_HEIGHT; ++y)
        {
            for (u32 z = 0; z < MC_WORLD_DEPTH; ++z)
            {
                for (u32 x = 0; x < MC_WORLD_WIDTH; ++x)
                {
                    Chunk& chunk = this->chunks[y][z][x];
                    chunk.coords = { x, y, z };
                    
                    glm::vec3 chunk_position = 
                        this->first_chunk_position + glm::vec3((f32)x, (f32)y, (f32)z) * chunk_size;

                    chunk.initialize(this, chunk_position);
                }
            }
        }

        return true;
    }

    Chunk* World::get_neighbour_chunk_from_right(const glm::ivec3& chunk_coords)
    {
        if (chunk_coords.x + 1 >= MC_WORLD_WIDTH)
        {
            return nullptr;
        }

        return &(this->chunks[chunk_coords.y][chunk_coords.z][chunk_coords.x + 1]);
    }
    
    Chunk* World::get_neighbour_chunk_from_left(const glm::ivec3& chunk_coords)
    {
        if (chunk_coords.x - 1 < 0)
        {
            return nullptr;
        }

        return &(this->chunks[chunk_coords.y][chunk_coords.z][chunk_coords.x - 1]);
    } 
    
    Chunk* World::get_neighbour_chunk_from_top(const glm::ivec3& chunk_coords)
    {
        if (chunk_coords.y + 1 >= MC_WORLD_HEIGHT)
        {
            return nullptr;
        }

        return &(this->chunks[chunk_coords.y + 1][chunk_coords.z][chunk_coords.x]);
    }

    Chunk* World::get_neighbour_chunk_from_bottom(const glm::ivec3& chunk_coords)
    {
        if (chunk_coords.y - 1 < 0)
        {
            return nullptr;
        }

        return &(this->chunks[chunk_coords.y - 1][chunk_coords.z][chunk_coords.x]);
    }

    Chunk* World::get_neighbour_chunk_from_front(const glm::ivec3& chunk_coords)
    {
        if (chunk_coords.z - 1 < 0)
        {
            return nullptr;
        }
        
        return &(this->chunks[chunk_coords.y][chunk_coords.z - 1][chunk_coords.x]);
    }
    
    Chunk* World::get_neighbour_chunk_from_back(const glm::ivec3& chunk_coords)
    {
        if (chunk_coords.z + 1 >= MC_WORLD_DEPTH)
        {
            return nullptr;
        }

        return &(this->chunks[chunk_coords.y][chunk_coords.z + 1][chunk_coords.x]);
    }

    const Block_Info World::block_infos[BlockId_Count] =
    {
        // Air
        {
            -1,
            -1,
            -1,
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