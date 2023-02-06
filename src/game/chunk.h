#pragma once

#include "core/common.h"
#include "memory/memory_arena.h"
#include "game/math.h"
#include "game/jobs.h"
#include "containers/string.h"
#include "containers/queue.h"

#include <array>
#include <atomic>

namespace minecraft {

    struct World;

    enum BlockId : u16
    {
        BlockId_Air                = 0,
        BlockId_Grass              = 1,
        BlockId_Sand               = 2,
        BlockId_Dirt               = 3,
        BlockId_Stone              = 4,
        BlockId_Green_Concrete     = 5,
        BlockId_Bedrock            = 6,
        BlockId_Oak_Log            = 7,
        BlockId_Oak_Leaves         = 8,
        BlockId_Oak_Planks         = 9,
        BlockId_Glow_Stone         = 10,
        BlockId_Cobbles_Stone      = 11,
        BlockId_Spruce_Log         = 12,
        BlockId_Spruce_Planks      = 13,
        BlockId_Glass              = 14,
        BlockId_Sea_Lantern        = 15,
        BlockId_Birch_Log          = 16,
        BlockId_Blue_Stained_Glass = 17,
        BlockId_Water              = 18,
        BlockId_Birch_Planks       = 19,
        BlockId_Diamond_Block      = 20,
        BlockId_Obsidian           = 21,
        BlockId_Crying_Obsidian    = 22,
        BlockId_Dark_Oak_Log       = 23,
        BlockId_Dark_Oak_Planks    = 24,
        BlockId_Jungle_Log         = 25,
        BlockId_Jungle_Planks      = 26,
        BlockId_Acacia_Log         = 27,
        BlockId_Acacia_Planks      = 28,
        BlockId_Count              = 29
    };

    enum BlockFlags : u32
    {
        BlockFlags_IsSolid            = 1,
        BlockFlags_IsTransparent      = 2,
        BlockFlags_ColorTopByBiome    = 4,
        BlockFlags_ColorSideByBiome   = 8,
        BlockFlags_ColorBottomByBiome = 16,
        BlockFlags_IsLightSource      = 32
    };

    enum BlockFace : u32
    {
        BlockFace_Top    = 0,
        BlockFace_Bottom = 1,
        BlockFace_Left   = 2,
        BlockFace_Right  = 3,
        BlockFace_Front  = 4,
        BlockFace_Back   = 5
    };

    enum BlockFaceCorner : u32
    {
        BlockFaceCorner_BottomRight  = 0,
        BlockFaceCorner_BottomLeft   = 1,
        BlockFaceCorner_TopLeft      = 2,
        BlockFaceCorner_TopRight     = 3
    };

    struct Block
    {
        u16 id;
    };

    struct Block_Light_Info
    {
        u8 sky_light_level;
        u8 light_source_level;
    };

    struct Block_Info
    {
        const char *name;
        u16         top_texture_id;
        u16         bottom_texture_id;
        u16         side_texture_id;
        u32         flags;
    };

    struct Block_Face_Info
    {
        glm::vec3 normal;
    };

    inline bool is_block_solid(const Block_Info *block_info)
    {
        return block_info->flags & BlockFlags_IsSolid;
    }

    inline bool is_block_transparent(const Block_Info *block_info)
    {
        return block_info->flags & BlockFlags_IsTransparent;
    }

    inline bool is_light_source(const Block_Info *block_info)
    {
        return block_info->flags & BlockFlags_IsLightSource;
    }

    inline bool should_color_top_by_biome(const Block_Info *block_info)
    {
        return block_info->flags & BlockFlags_ColorTopByBiome;
    }

    inline bool should_color_side_by_biome(const Block_Info *block_info)
    {
        return block_info->flags & BlockFlags_ColorSideByBiome;
    }

    inline bool should_color_bottom_by_biome(const Block_Info *block_info)
    {
        return block_info->flags & BlockFlags_ColorBottomByBiome;
    }

    enum BlockNeighbour
    {
        BlockNeighbour_Up    = 0,
        BlockNeighbour_Down  = 1,
        BlockNeighbour_Left  = 2,
        BlockNeighbour_Right = 3,
        BlockNeighbour_Front = 4,
        BlockNeighbour_Back  = 5,
    };

    enum ChunkNeighbour
    {
        ChunkNeighbour_Front      = 0,
        ChunkNeighbour_Back       = 1,
        ChunkNeighbour_Left       = 2,
        ChunkNeighbour_Right      = 3,
        ChunkNeighbour_FrontRight = 4,
        ChunkNeighbour_FrontLeft  = 5,
        ChunkNeighbour_BackRight  = 6,
        ChunkNeighbour_BackLeft   = 7,
        ChunkNeighbour_Count      = 8
    };

    enum ChunkState : u8
    {
        ChunkState_Initialized                = 0,
        ChunkState_Loaded                     = 1,
        ChunkState_NeighboursLoaded           = 2,
        ChunkState_PendingForLightPropagation = 3,
        ChunkState_LightPropagated            = 4,
        ChunkState_PendingForLightCalculation = 5,
        ChunkState_LightCalculated            = 6,
        ChunkState_PendingForSave             = 8,
        ChunkState_Saved                      = 9,
        ChunkState_Freed                      = 10
    };

    enum TessellationState : u8
    {
        TessellationState_None    = 0,
        TessellationState_Pending = 1,
        TessellationState_Done    = 2
    };

    struct Block_Face_Vertex
    {
        u32 packed_vertex_attributes0;
        u32 packed_vertex_attributes1;
    };

    struct Chunk_Instance
    {
        glm::ivec2 chunk_coords;
    };

    struct Sub_Chunk_Bucket
    {
        i32                memory_id;
        Block_Face_Vertex *current_vertex;
        i32                face_count;
    };

    bool initialize_sub_chunk_bucket(Sub_Chunk_Bucket *sub_chunk_bucket);
    bool is_sub_chunk_bucket_allocated(const Sub_Chunk_Bucket *sub_chunk_bucket);

    struct Sub_Chunk_Render_Data
    {
        i32             instance_memory_id;
        Chunk_Instance *base_instance;

        AABB aabb[2];

        std::atomic< i32 > bucket_index;
        Sub_Chunk_Bucket opaque_buckets[2];
        Sub_Chunk_Bucket transparent_buckets[2];

        std::atomic< TessellationState > state;

        i32 face_count;
    };

    struct Chunk
    {
        constexpr static i32 Width  = 16;
        constexpr static i32 Height = 256;
        constexpr static i32 Depth  = 16;
        constexpr static u64 SubChunkHeight = 8;

        static_assert(Height % SubChunkHeight == 0);
        static constexpr u64 SubChunkCount = Height / SubChunkHeight;

        static constexpr u64 SubChunkBlockCount  = Width * Depth * SubChunkHeight;
        static constexpr u64 SubChunkVertexCount = SubChunkBlockCount * 24;
        static constexpr u64 SubChunkIndexCount  = SubChunkBlockCount * 36;

        static constexpr std::array< glm::ivec2, ChunkNeighbour_Count > NeighbourDirections =
        {
            glm::ivec2 {  0, -1 },
            glm::ivec2 {  0,  1 },
            glm::ivec2 { -1,  0 },
            glm::ivec2 {  1,  0 },
            glm::ivec2 {  1, -1 },
            glm::ivec2 { -1, -1 },
            glm::ivec2 {  1,  1 },
            glm::ivec2 { -1,  1 }
        };

        glm::ivec2 world_coords;
        glm::vec3  position;

        Chunk *neighbours[ChunkNeighbour_Count];

        std::atomic< ChunkState >        state;
        std::atomic< TessellationState > tessellation_state;

        Block blocks[Chunk::Height * Chunk::Depth * Chunk::Width];
        Block front_edge_blocks[Chunk::Height * Chunk::Width];
        Block back_edge_blocks[Chunk::Height  * Chunk::Width];
        Block left_edge_blocks[Chunk::Height  * Chunk::Depth];
        Block right_edge_blocks[Chunk::Height * Chunk::Depth];

        Block_Light_Info light_map[Chunk::Height * Chunk::Depth * Chunk::Width];
        Block_Light_Info front_edge_light_map[Chunk::Height * Chunk::Width];
        Block_Light_Info back_edge_light_map[Chunk::Height  * Chunk::Width];
        Block_Light_Info left_edge_light_map[Chunk::Height  * Chunk::Depth];
        Block_Light_Info right_edge_light_map[Chunk::Height * Chunk::Depth];

        Sub_Chunk_Render_Data sub_chunks_render_data[Chunk::SubChunkCount];
    };

    i32 get_block_index(const glm::ivec3& block_coords);
    glm::vec3 get_block_position(Chunk *chunk, const glm::ivec3& block_coords);
    Block* get_block(Chunk *chunk, const glm::ivec3& block_coords);
    Block_Light_Info* get_block_light_info(Chunk *chunk, const glm::ivec3& block_coords);
    glm::ivec2 world_position_to_chunk_coords(const glm::vec3& position);

    bool initialize_chunk(Chunk *chunk,
                        const glm::ivec2 &world_coords);

    void generate_chunk(Chunk *chunk, i32 seed);

        void serialize_chunk(World *world,
                         Chunk *chunk,
                         i32 seed,
                         Temprary_Memory_Arena *temp_arena);

    void deserialize_chunk(World *world,
                           Chunk *chunk,
                           Temprary_Memory_Arena *temp_arena);

    void propagate_sky_light(World *world,
                             Chunk *chunk,
                             Circular_Queue< struct Block_Query_Result > *queue);

    void calculate_lighting(World *world,
                            Chunk *chunk,
                            Circular_Queue< struct Block_Query_Result > *queue);

    Block* get_neighbour_block_from_right(Chunk  *chunk, const glm::ivec3& block_coords);
    Block* get_neighbour_block_from_left(Chunk   *chunk, const glm::ivec3& block_coords);
    Block* get_neighbour_block_from_top(Chunk    *chunk, const glm::ivec3& block_coords);
    Block* get_neighbour_block_from_bottom(Chunk *chunk, const glm::ivec3& block_coords);
    Block* get_neighbour_block_from_front(Chunk  *chunk, const glm::ivec3& block_coords);
    Block* get_neighbour_block_from_back(Chunk   *chunk, const glm::ivec3& block_coords);
    std::array< Block*, 6 > get_neighbours(Chunk *chunk, const glm::ivec3& block_coords);

    String8 get_chunk_file_path(World                 *world,
                                Chunk                 *chunk,
                                Temprary_Memory_Arena *temp_arena);

    inline i64 get_chunk_hash(const glm::ivec2& coords)
    {
        // note(harlequin): hash function from https://carmencincotti.com/2022-10-31/spatial-hash-maps-part-one/
        // return glm::abs((i64)coords.x | ((i64)coords.y << (i64)32));
        return glm::abs(((i64)coords.x * (i64)92837111) ^ ((i64)coords.y * (i64)689287499));
    }

    inline i32 get_sub_chunk_render_data_index(const glm::ivec3& block_coords)
    {
        return block_coords.y / Chunk::SubChunkHeight;
    }
}
