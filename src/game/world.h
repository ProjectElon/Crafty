#pragma once

#include "core/common.h"

#include "meta/spritesheet_meta.h"
#include "memory/free_list.h"

#include <glm/glm.hpp>

#include <mutex>
#include <algorithm>
#include <unordered_map> // todo(harlequin): containers

#define MC_CHUNK_HEIGHT 256
#define MC_CHUNK_DEPTH  16
#define MC_CHUNK_WIDTH  16

#define MC_BLOCK_COUNT_PER_CHUNK MC_CHUNK_WIDTH * MC_CHUNK_HEIGHT * MC_CHUNK_DEPTH
#define MC_INDEX_COUNT_PER_CHUNK MC_BLOCK_COUNT_PER_CHUNK * 36

#define MC_SUB_CHUNK_HEIGHT 8
#define MC_BLOCK_COUNT_PER_SUB_CHUNK  MC_CHUNK_WIDTH * MC_CHUNK_DEPTH * MC_SUB_CHUNK_HEIGHT
#define MC_VERTEX_COUNT_PER_SUB_CHUNK MC_BLOCK_COUNT_PER_SUB_CHUNK * 24
#define MC_INDEX_COUNT_PER_SUB_CHUNK  MC_BLOCK_COUNT_PER_SUB_CHUNK * 36

#define MC_SUB_CHUNK_VERTEX_COUNT 2048

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

    struct Block
    {
        u16 id;
    };

    struct Sub_Chunk_Vertex
    {
        u32 data0;
        u32 data1;
    };

    struct Sub_Chunk_Instance
    {
        glm::ivec2 chunk_coords;
    };

    struct Sub_Chunk_Render_Data
    {
        Sub_Chunk_Vertex vertices[MC_SUB_CHUNK_VERTEX_COUNT];

        i32 vertex_count;
        i32 face_count;

        i32 id;
        Sub_Chunk_Vertex   *base_vertex;
        Sub_Chunk_Instance *base_instance;

        bool uploaded_to_gpu;
    };

    struct Chunk
    {
        glm::ivec2 world_coords;
        glm::vec3 position;

        bool pending;
        bool loaded;

        Block blocks[MC_CHUNK_HEIGHT * MC_CHUNK_DEPTH * MC_CHUNK_WIDTH];
        Block front_edge_blocks[MC_CHUNK_HEIGHT * MC_CHUNK_WIDTH];
        Block back_edge_blocks[MC_CHUNK_HEIGHT  * MC_CHUNK_WIDTH];
        Block left_edge_blocks[MC_CHUNK_HEIGHT  * MC_CHUNK_DEPTH];
        Block right_edge_blocks[MC_CHUNK_HEIGHT * MC_CHUNK_DEPTH];

        Sub_Chunk_Render_Data sub_chunks_render_data[MC_CHUNK_HEIGHT / MC_SUB_CHUNK_HEIGHT];

        bool initialize(const glm::ivec2 &world_coords);

        void generate(i32 seed);

        void serialize();
        void deserialize();

        inline i32 get_block_index(const glm::ivec3& block_coords) const
        {
            return block_coords.y * MC_CHUNK_WIDTH * MC_CHUNK_DEPTH + block_coords.z * MC_CHUNK_WIDTH + block_coords.x;
        }

        inline glm::vec3 get_block_position(const glm::ivec3& block_coords) const
        {
            return this->position + glm::vec3((f32)block_coords.x + 0.5f, (f32)block_coords.y + 0.5f, (f32)block_coords.z + 0.5f);
        }

        Block* get_block(const glm::ivec3& block_coords);

        Block* Chunk::get_neighbour_block_from_right(const glm::ivec3& block_coords);
        Block* Chunk::get_neighbour_block_from_left(const glm::ivec3& block_coords);
        Block* Chunk::get_neighbour_block_from_top(const glm::ivec3& block_coords);
        Block* Chunk::get_neighbour_block_from_bottom(const glm::ivec3& block_coords);
        Block* Chunk::get_neighbour_block_from_front(const glm::ivec3& block_coords);
        Block* Chunk::get_neighbour_block_from_back(const glm::ivec3& block_coords);
    };

    struct Chunk_Hash
    {
        std::size_t operator ()(const glm::ivec2& coord) const
        {
            return (size_t)coord.x | ((size_t)coord.y << (size_t)32);
        }
    };

    struct Block_Query_Result
    {
        glm::ivec3 block_coords;
        Block *block;
        Chunk *chunk;
    };

    struct World
    {
        static const Block_Info block_infos[BlockId_Count];  // todo(harlequin): this is going to be content driven in the future with the help of a tool
        static Block null_block;
        
        static std::unordered_map< glm::ivec2, Chunk*, Chunk_Hash > loaded_chunks;
        static std::string path;
        static i32 seed;

        static constexpr i64 sub_chunk_height = MC_SUB_CHUNK_HEIGHT;
        static constexpr i64 sub_chunk_count_per_chunk = MC_CHUNK_HEIGHT / sub_chunk_height;

        static constexpr i64 chunk_radius = 12;
        static constexpr i64 chunk_capacity = 4 * (chunk_radius + 2) * (chunk_radius + 2);

        static constexpr i64 sub_chunk_capacity = sub_chunk_count_per_chunk * chunk_capacity;
        static constexpr i64 max_vertex_count_per_sub_chunk = MC_SUB_CHUNK_VERTEX_COUNT;
        static constexpr i64 sub_chunk_size = sizeof(Sub_Chunk_Vertex) * max_vertex_count_per_sub_chunk;

        static std::mutex chunk_pool_mutex;
        static minecraft::Free_List<minecraft::Chunk, chunk_capacity> chunk_pool;

        static inline glm::ivec2 world_position_to_chunk_coords(const glm::vec3& position)
        {
            const f32 one_over_16 = 1.0f / 16.0f;
            return { glm::floor(position.x * one_over_16), glm::floor(position.z * one_over_16) };
        }

        static inline glm::ivec3 world_position_to_block_coords(const glm::vec3& position)
        {
            glm::ivec2 chunk_coords = world_position_to_chunk_coords(position);
            glm::vec3 offset = position - glm::vec3(chunk_coords.x * 16.0f, position.y, chunk_coords.y * 16.0f);
            return { (i32)glm::floor(offset.x), (i32)glm::floor(position.y), (i32)glm::floor(offset.z)};
        }

        static inline Chunk* get_chunk(const glm::ivec2& chunk_coords)
        {
            auto it = loaded_chunks.find({ chunk_coords.x, chunk_coords.y });
            Chunk *chunk = nullptr;
            if (it != loaded_chunks.end())
            {
                chunk = it->second;
            }
            return chunk;
        }

        static inline Block* get_block(const glm::vec3& position)
        {
           glm::ivec2 chunk_coords = world_position_to_chunk_coords(position);
           Chunk* chunk = get_chunk(chunk_coords);
           if (chunk)
           {
                glm::ivec3 block_coords = world_position_to_block_coords(position);
                return chunk->get_block(block_coords);
           }
           return &World::null_block;
        }

        static inline Block_Query_Result query_block(const glm::vec3& position)
        {
            glm::ivec2 chunk_coords = world_position_to_chunk_coords(position);
            Chunk* chunk = get_chunk(chunk_coords);
            glm::ivec3 block_coords = world_position_to_block_coords(position);
            if (chunk)
            {
                if (block_coords.y < 0 || block_coords.y >= MC_CHUNK_HEIGHT)
                {
                    return { block_coords, &World::null_block, chunk };
                }
                return { block_coords, chunk->get_block(block_coords), chunk };
            }
            return { block_coords, &World::null_block, nullptr };
        }

        static inline i32 get_sub_chunk_index(const glm::ivec3& block_coords)
        {
            return block_coords.y / sub_chunk_height;
        }

        static Block_Query_Result get_neighbour_block_from_top(Chunk *chunk, const glm::ivec3& block_coords);
        static Block_Query_Result get_neighbour_block_from_bottom(Chunk *chunk, const glm::ivec3& block_coords);

        static Block_Query_Result get_neighbour_block_from_left(Chunk *chunk, const glm::ivec3& block_coords);
        static Block_Query_Result get_neighbour_block_from_right(Chunk *chunk, const glm::ivec3& block_coords);

        static Block_Query_Result get_neighbour_block_from_front(Chunk *chunk, const glm::ivec3& block_coords);
        static Block_Query_Result get_neighbour_block_from_back(Chunk *chunk, const glm::ivec3& block_coords);

        static void set_block_id(Chunk *chunk, const glm::ivec3& block_coords, u16 block_id);

        static std::string get_chunk_path(Chunk *chunk);
    };
}