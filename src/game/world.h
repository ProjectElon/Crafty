#pragma once

#include "core/common.h"

#include "meta/spritesheet_meta.h"

#include <glm/glm.hpp>

namespace minecraft {

    enum BlockId : u32
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
        u32 top_texture_id;
        u32 bottom_texture_id;
        u32 side_texture_id;
        u32 flags;
    };

    struct Block
    {
        u32 id;
    };

    #define MC_CHUNK_HEIGHT 15
    #define MC_CHUNK_DEPTH  15
    #define MC_CHUNK_WIDTH  15

    struct World;

    struct Chunk
    {
        World *world;
        glm::ivec3 coords;

        glm::vec3 position;
        glm::vec3 first_block_position; // top - left - bottom

        Block blocks[MC_CHUNK_HEIGHT][MC_CHUNK_DEPTH][MC_CHUNK_WIDTH]; // blocks

        bool initialize(World* world, const glm::vec3& position);

        inline Block* get_block(const glm::ivec3& block_coords) // z => row, x => colomn, y => height
        {
            assert(
                block_coords.x >= 0 && block_coords.x < MC_CHUNK_WIDTH && 
                block_coords.y >= 0 && block_coords.y < MC_CHUNK_HEIGHT && 
                block_coords.z >= 0 && block_coords.z < MC_CHUNK_DEPTH); // todo(harlequin): add assert macro
            return &(this->blocks[coords.y][coords.z][coords.x]);
        }

        inline glm::vec3 get_block_position(const glm::ivec3& block_coords) const // z => row, x => colomn, y => height
        {
            glm::vec3 block_size = { 1.0f, 1.0f, 1.0f }; 
            return this->first_block_position + glm::vec3((f32)block_coords.x, (f32)block_coords.y, (f32)block_coords.z) * block_size;
        }

        Block* Chunk::get_neighbour_block_from_right(const glm::ivec3& block_coords);
        Block* Chunk::get_neighbour_block_from_left(const glm::ivec3& block_coords);
        Block* Chunk::get_neighbour_block_from_top(const glm::ivec3& block_coords);
        Block* Chunk::get_neighbour_block_from_bottom(const glm::ivec3& block_coords);
        Block* Chunk::get_neighbour_block_from_front(const glm::ivec3& block_coords);
        Block* Chunk::get_neighbour_block_from_back(const glm::ivec3& block_coords);
    };

#define MC_WORLD_HEIGHT 3
#define MC_WORLD_DEPTH 3
#define MC_WORLD_WIDTH 3

    struct World
    {
        static const Block_Info block_infos[BlockId_Count];  // todo(harlequin): this is going to be content driven in the future with the help of a tool
        static Block null_block;

        glm::vec3 position;
        glm::vec3 first_chunk_position;

        Chunk chunks[MC_WORLD_HEIGHT][MC_WORLD_DEPTH][MC_WORLD_WIDTH];

        bool initialize(const glm::vec3& position = { 0.0f, 0.0f, 0.0f });

        inline Chunk* get_chunk(const glm::ivec3& chunk_coords)
        {
            assert(
                chunk_coords.x >= 0 && chunk_coords.x < MC_WORLD_WIDTH && 
                chunk_coords.y >= 0 && chunk_coords.y < MC_WORLD_HEIGHT && 
                chunk_coords.z >= 0 && chunk_coords.z < MC_WORLD_DEPTH); // todo(harlequin): add assert macro
            return &(this->chunks[chunk_coords.y][chunk_coords.z][chunk_coords.x]);
        }

        inline glm::vec3 get_chunk_position(const glm::ivec3& chunk_coords)
        {
            assert(
                chunk_coords.x >= 0 && chunk_coords.x < MC_WORLD_WIDTH && 
                chunk_coords.y >= 0 && chunk_coords.y < MC_WORLD_HEIGHT && 
                chunk_coords.z >= 0 && chunk_coords.z < MC_WORLD_DEPTH); // todo(harlequin): add assert macro
            
            glm::vec3 block_size = { 1.0f, 1.0f, 1.0f };
            glm::vec3 chunk_size = glm::vec3(MC_CHUNK_WIDTH, MC_CHUNK_HEIGHT, MC_CHUNK_DEPTH) * block_size;
            return this->first_chunk_position + glm::vec3((f32)chunk_coords.x, (f32)chunk_coords.y, (f32)chunk_coords.z) * chunk_size;
        }

        Chunk* World::get_neighbour_chunk_from_right(const glm::ivec3& chunk_coords);
        Chunk* World::get_neighbour_chunk_from_left(const glm::ivec3& chunk_coords);
        Chunk* World::get_neighbour_chunk_from_top(const glm::ivec3& chunk_coords);
        Chunk* World::get_neighbour_chunk_from_bottom(const glm::ivec3& chunk_coords);
        Chunk* World::get_neighbour_chunk_from_front(const glm::ivec3& chunk_coords);
        Chunk* World::get_neighbour_chunk_from_back(const glm::ivec3& chunk_coords);
    };
}