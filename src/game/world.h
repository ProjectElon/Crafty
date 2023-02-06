#pragma once

#include "core/common.h"

#include "meta/spritesheet_meta.h"
#include "game/chunk.h"
#include "memory/memory_arena.h"
#include "game/math.h"
#include "game/jobs.h"
#include "containers/string.h"
#include "containers/queue.h"

#include <glm/glm.hpp>

#include <array>

namespace minecraft {

    struct World_Region_Bounds
    {
        glm::ivec2 min;
        glm::ivec2 max;
    };

    struct Chunk_Node
    {
        Chunk       chunk;
        Chunk_Node *next;
    };

    enum ChunkHashTableEntryState : u16
    {
        ChunkHashTableEntryState_Empty    = 0x0,
        ChunkHashTableEntryState_Deleted  = 0x1,
        ChunkHashTableEntryState_Occupied = 0x2
    };

    struct Block_Query_Result
    {
        glm::ivec3  block_coords;
        Block      *block;
        Chunk      *chunk;
    };

    struct Select_Block_Result
    {
        Block_Query_Result block_query;
        Block_Query_Result block_facing_normal_query;
        Ray_Cast_Result    ray_cast_result;
        BlockFace          face;
        glm::vec3          block_position;
        glm::vec3          block_facing_normal_position;
        glm::vec3          normal;
    };

    struct World
    {
        static constexpr i64 MaxChunkRadius            = 30;
        static constexpr i64 PendingFreeChunkRadius    = 2;
        static constexpr i64 ChunkCapacity             = 4 * (MaxChunkRadius + PendingFreeChunkRadius) * (MaxChunkRadius + PendingFreeChunkRadius);
        static constexpr i64 SubChunkBucketCapacity    = 4 * ChunkCapacity;
        static constexpr i64 SubChunkBucketFaceCount   = 1024;
        static constexpr i64 SubChunkBucketVertexCount = 4 * SubChunkBucketFaceCount;
        static constexpr i64 SubChunkBucketSize        = SubChunkBucketVertexCount * sizeof(Block_Face_Vertex);

        f32 game_time_rate;
        f32 game_timer;
        u32 game_time;

        f32 sky_light_level;

        String8             path;
        i32                 seed;
        World_Region_Bounds active_region_bounds;

        static Block            null_block;
        static const Block_Info block_infos[BlockId_Count];  // todo(harlequin): this is going to be content driven in the future with the help of a tool

        u32         free_chunk_count;
        Chunk_Node  chunk_nodes[ChunkCapacity];
        Chunk_Node *first_free_chunk_node;

        constexpr static u16 ChunkHashTableEntryStateMask = 0xC000;
        constexpr static u16 ChunkHashTableEntryValueMask = 0x0FFF;
        constexpr static i64 ChunkHashTableCapacity       = ChunkCapacity;

        glm::ivec2 chunk_hash_table_keys[ChunkHashTableCapacity];
        u16        chunk_hash_table_values[ChunkHashTableCapacity];

        Circular_Queue< Update_Chunk_Job >                      update_chunk_jobs_queue;
        Circular_Queue< Calculate_Chunk_Light_Propagation_Job > light_propagation_queue;
        Circular_Queue< Calculate_Chunk_Lighting_Job >          calculate_chunk_lighting_queue;
    };

    inline ChunkHashTableEntryState get_entry_state(u16 entry)
    {
        return (ChunkHashTableEntryState)(entry >> 14);
    }

    inline void set_entry_state(u16 &entry, ChunkHashTableEntryState state)
    {
        entry = (entry & (~World::ChunkHashTableEntryStateMask)) | (state << 14);
    }

    inline u16 get_entry_value(u16 entry)
    {
        return entry & World::ChunkHashTableEntryValueMask;
    }

    inline void set_entry_value(u16 &entry, u16 value)
    {
        entry = (entry & (~World::ChunkHashTableEntryValueMask)) | (value);
    }

    bool initialize_world(World *world,
                          String8 path,
                          Temprary_Memory_Arena *temp_arena);

    void shutdown_world(World *world);

    void update_world_time(World *world, f32 delta_time);

    u32 real_time_to_game_time(u32 hours,
                               u32 minutes,
                               u32 seconds);

    void game_time_to_real_time(u32  game_time,
                                u32 *out_hours,
                                u32 *out_minutes,
                                u32 *out_seconds);


    Chunk *allocate_chunk(World *world);
    void free_chunk(World *world, Chunk *chunk);

    Chunk *insert_and_allocate_chunk(World            *world,
                                     const glm::ivec2 &chunk_coords);

    Chunk *get_chunk(World *world, const glm::ivec2& coords);

    bool remove_chunk(World *world, const glm::ivec2& coords);

    void load_and_update_chunks(World *world, const World_Region_Bounds& region_bounds);

    glm::ivec3 world_position_to_block_coords(World *world, const glm::vec3& position);
    World_Region_Bounds get_world_bounds_from_chunk_coords(i32 chunk_radius, const glm::ivec2& chunk_coords);
    bool is_chunk_in_region_bounds(const glm::ivec2& chunk_coords, const World_Region_Bounds& region_bounds);
    bool is_block_query_valid(const Block_Query_Result& query);
    bool is_block_query_in_world_region(const Block_Query_Result& query, const World_Region_Bounds& bounds);

    Block* get_block(World *world, const glm::vec3& position);
    Block_Query_Result query_block(World *world, const glm::vec3& position);

    Block_Query_Result query_neighbour_block_from_top(Chunk *chunk,    const glm::ivec3& block_coords);
    Block_Query_Result query_neighbour_block_from_bottom(Chunk *chunk, const glm::ivec3& block_coords);

    Block_Query_Result query_neighbour_block_from_left(Chunk *chunk,  const glm::ivec3& block_coords);
    Block_Query_Result query_neighbour_block_from_right(Chunk *chunk, const glm::ivec3& block_coords);

    Block_Query_Result query_neighbour_block_from_front(Chunk *chunk, const glm::ivec3& block_coords);
    Block_Query_Result query_neighbour_block_from_back(Chunk *chunk,  const glm::ivec3& block_coords);

    std::array< Block_Query_Result, 6 > query_neighbours(Chunk *chunk, const glm::ivec3& block_coords);

    Select_Block_Result select_block(World          *world,
                                    const glm::vec3 &view_position,
                                    const glm::vec3 &view_direction,
                                    u32              max_block_select_dist_in_cube_units);

    void set_block_id(Chunk *chunk, const glm::ivec3& block_coords, u16 block_id);
    void set_block_sky_light_level(World *world, Chunk *chunk, const glm::ivec3& block_coords, u8 light_level);
    void set_block_light_source_level(World *world, Chunk *chunk, const glm::ivec3& block_coords, u8 light_level);

    inline const Block_Info* get_block_info(World *world, const Block *block)
    {
        return &world->block_infos[block->id];
    }

    inline u8 get_sky_light_level(World *world, const Block_Light_Info *block_light_info)
    {
        i32 sky_light_factor = (i32)world->sky_light_level - 15;
        return (u8) glm::max(block_light_info->sky_light_level + sky_light_factor, 1);
    }

    inline u8 get_light_level(World *world, const Block_Light_Info *block_light_info)
    {
        return glm::max(get_sky_light_level(world, block_light_info), block_light_info->light_source_level);
    }

    void save_chunks(World *world);
}