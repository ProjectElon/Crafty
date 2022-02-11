#pragma once

#include "core/common.h"

#include "meta/spritesheet_meta.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <unordered_map> // todo(harlequin): containers

namespace minecraft {

    enum BlockId : u16
    {
        BlockId_Air   = 0,
        BlockId_Grass = 1,
        BlockId_Sand  = 2,
        BlockId_Dirt  = 3,
        BlockId_Stone = 4,
        BlockId_Green_Concrete = 5,
        BlockId_Bedrock = 6,
        BlockId_Oak_Log = 7,
        BlockId_Oak_Leaves = 8,
        BlockId_Oak_Planks = 9,
        BlockId_Glow_Stone = 10,
        BlockId_Cobbles_Stone = 11,
        BlockId_Spruce_Log = 12,
        BlockId_Spruce_Planks = 13,
        BlockId_Glass = 14,
        BlockId_Sea_Lantern = 15,
        BlockId_Birch_Log = 16,
        BlockId_Blue_Stained_Glass = 17,
        BlockId_Water = 18,
        BlockId_Birch_Planks = 19,
        BlockId_Diamond_Block = 20,
        BlockId_Obsidian = 21,
        BlockId_Crying_Obsidian = 22,
        BlockId_Dark_Oak_Log = 23,
        BlockId_Dark_Oak_Planks = 24,
        BlockId_Jungle_Log = 25,
        BlockId_Jungle_Planks = 26,
        BlockId_Acacia_Log = 27,
        BlockId_Acacia_Planks = 28,
        BlockId_Count = 29
    };
    
    enum BlockFlags : u32
    {
        BlockFlags_Is_Solid = 1,
        BlockFlags_Is_Transparent = 2,
        BlockFlags_Should_Color_Top_By_Biome = 4,
        BlockFlags_Should_Color_Side_By_Biome = 8,
        BlockFlags_Should_Color_Bottom_By_Biome = 16
    };

    struct Block_Info
    {
        u16 top_texture_id;
        u16 bottom_texture_id;
        u16 side_texture_id;
        u32 flags;

        inline bool is_solid() const
        {
            return flags & BlockFlags_Is_Solid;
        }

        inline bool is_transparent() const
        {
            return flags & BlockFlags_Is_Transparent;
        }

        inline bool should_color_top_by_biome() const
        {
            return flags & BlockFlags_Should_Color_Top_By_Biome;
        }

        inline bool should_color_side_by_biome() const
        {
            return flags & BlockFlags_Should_Color_Side_By_Biome;
        }

        inline bool should_color_bottom_by_biome() const
        {
            return flags & BlockFlags_Should_Color_Bottom_By_Biome;
        }
    };

    struct Block_Vertex
    {
        glm::vec3 position;
        glm::vec2 uv;
        u32 flags;
    };

    struct Block
    {
        u16 id;
    };

    #define MC_CHUNK_HEIGHT 256
    #define MC_CHUNK_DEPTH  16
    #define MC_CHUNK_WIDTH  16

    struct Chunk_Render_Data
    {
        bool pending;
        
        Block_Vertex vertices[MC_CHUNK_WIDTH * MC_CHUNK_HEIGHT * MC_CHUNK_DEPTH * 24];
        Block_Vertex *current_vertex_pointer;
        u32 face_count;
    };

    // todo(harlequin): block_coords comperssion inside chunk
    struct Chunk
    {
        glm::ivec2 world_coords;

        glm::vec3 position;
        glm::vec3 first_block_position; // top - left - bottom

        Block blocks[MC_CHUNK_HEIGHT * MC_CHUNK_DEPTH * MC_CHUNK_WIDTH];
        Chunk_Render_Data render_data;

        bool initialize(const glm::ivec2 &world_coords);
        void generate(u32 seed);

        inline u32 get_block_index(const glm::ivec3& block_coords) const
        {
            return block_coords.y * MC_CHUNK_WIDTH * MC_CHUNK_DEPTH + block_coords.z * MC_CHUNK_WIDTH + block_coords.x;
        }

        inline Block* get_block(const glm::ivec3& block_coords)
        {
            u32 block_index = get_block_index(block_coords);
            return &(this->blocks[block_index]);
        }

        inline glm::vec3 get_block_position(const glm::ivec3& block_coords) const
        {
            return this->first_block_position + glm::vec3((f32)block_coords.x, (f32)block_coords.y, (f32)block_coords.z);
        }

        Block* Chunk::get_neighbour_block_from_right(const glm::ivec3& block_coords);
        Block* Chunk::get_neighbour_block_from_left(const glm::ivec3& block_coords);
        Block* Chunk::get_neighbour_block_from_top(const glm::ivec3& block_coords);
        Block* Chunk::get_neighbour_block_from_bottom(const glm::ivec3& block_coords);
        Block* Chunk::get_neighbour_block_from_front(const glm::ivec3& block_coords);
        Block* Chunk::get_neighbour_block_from_back(const glm::ivec3& block_coords);
    };

    #define MC_WORLD_WIDTH 11
    #define MC_WORLD_DEPTH 11

    struct World
    {
        static const Block_Info block_infos[BlockId_Count];  // todo(harlequin): this is going to be content driven in the future with the help of a tool
        static Block null_block;
        
        static std::unordered_map< i64, Chunk* > loaded_chunks;
        
        static inline glm::ivec2 world_position_to_chunk_coords(const glm::vec3& position)
        {
            const f32 one_over_16 = 1.0f / 16.0f;
            return { position.x * one_over_16, position.z * one_over_16 };
        }

        static inline Chunk* get_chunk(const glm::ivec2& chunk_coords)
        {
            i64 chunk_hash = chunk_coords.y * MC_CHUNK_WIDTH + chunk_coords.x; 
            auto it = loaded_chunks.find(chunk_hash);
            Chunk *chunk = nullptr;
            if (it != loaded_chunks.end())
            {
                chunk = it->second;
            }
            return chunk;
        }

        static inline Chunk* get_neighbour_chunk_from_right(const glm::ivec2& chunk_coords)
        {
            return get_chunk({ chunk_coords.x + 1, chunk_coords.y });
        }
        
        static inline Chunk* get_neighbour_chunk_from_left(const glm::ivec2& chunk_coords)
        {
            return get_chunk({ chunk_coords.x - 1, chunk_coords.y });
        }
        
        static inline Chunk* get_neighbour_chunk_from_front(const glm::ivec2& chunk_coords)
        {
            return get_chunk({ chunk_coords.x, chunk_coords.y - 1 });
        }
        
        static inline Chunk* get_neighbour_chunk_from_back(const glm::ivec2& chunk_coords)
        {
            return get_chunk({ chunk_coords.x, chunk_coords.y + 1 });
        }

        static inline Block* get_neighbour_block_from_right(Chunk* chunk, const glm::ivec3& block_coords)
        {
            if (block_coords.x + 1 >= MC_CHUNK_WIDTH) 
            {
                Chunk *right_chunk = get_neighbour_chunk_from_right(chunk->world_coords);
                
                if (right_chunk)
                {
                    return right_chunk->get_block({ 0, block_coords.y, block_coords.z });
                }
                else
                {
                    return &World::null_block;
                }
            }

            return chunk->get_block({ block_coords.x + 1, block_coords.y, block_coords.z });
        }

        static inline Block* get_neighbour_block_from_left(Chunk* chunk, const glm::ivec3& block_coords)
        {
            if (block_coords.x - 1 < 0) 
            {
                Chunk *left_chunk = get_neighbour_chunk_from_left(chunk->world_coords);
                
                if (left_chunk)
                {
                    return left_chunk->get_block({ MC_CHUNK_WIDTH - 1, block_coords.y, block_coords.z });
                }
                else
                {
                    return &World::null_block;
                }
            }

            return chunk->get_block({ block_coords.x - 1, block_coords.y, block_coords.z });
        }
        
        static inline Block* get_neighbour_block_from_top(Chunk* chunk, const glm::ivec3& block_coords)
        {
            return chunk->get_neighbour_block_from_top(block_coords);
        }

        static inline Block* get_neighbour_block_from_bottom(Chunk* chunk, const glm::ivec3& block_coords)
        {
            return chunk->get_neighbour_block_from_bottom(block_coords);
        }
        
        static inline Block* get_neighbour_block_from_front(Chunk* chunk, const glm::ivec3& block_coords)
        {
            if (block_coords.z - 1 < 0) 
            {
                Chunk *front_chunk = get_neighbour_chunk_from_front(chunk->world_coords);
                
                if (front_chunk)
                {
                    return front_chunk->get_block({ block_coords.x, block_coords.y, MC_CHUNK_DEPTH - 1 });
                }
                else
                {
                    return &World::null_block;
                }
            }

            return chunk->get_block({ block_coords.x, block_coords.y, block_coords.z - 1 });
        }
        
        static inline Block* get_neighbour_block_from_back(Chunk* chunk, const glm::ivec3& block_coords)
        {
            if (block_coords.z + 1 >= MC_CHUNK_DEPTH) 
            {
                Chunk *back_chunk = get_neighbour_chunk_from_back(chunk->world_coords);
                
                if (back_chunk)
                {
                    return back_chunk->get_block({ block_coords.x, block_coords.y, 0 });
                }
                else
                {
                    return &World::null_block;
                }
            }

            return chunk->get_block({ block_coords.x, block_coords.y, block_coords.z + 1 });
        }
    };
}