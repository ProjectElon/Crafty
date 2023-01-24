#pragma once

#include "core/common.h"

#include "meta/spritesheet_meta.h"
#include "memory/memory_arena.h"
#include "game/math.h"
#include "game/jobs.h"
#include "containers/string.h"
#include "containers/queue.h"

#include <glm/glm.hpp>

#include <mutex>
#include <array>
#include <atomic>

#define CHUNK_HEIGHT 256
#define CHUNK_DEPTH  16
#define CHUNK_WIDTH  16

#define BLOCK_COUNT_PER_CHUNK CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH
#define INDEX_COUNT_PER_CHUNK BLOCK_COUNT_PER_CHUNK * 36

#define SUB_CHUNK_HEIGHT 8
#define BLOCK_COUNT_PER_SUB_CHUNK  CHUNK_WIDTH * CHUNK_DEPTH * SUB_CHUNK_HEIGHT
#define VERTEX_COUNT_PER_SUB_CHUNK BLOCK_COUNT_PER_SUB_CHUNK * 24
#define INDEX_COUNT_PER_SUB_CHUNK  BLOCK_COUNT_PER_SUB_CHUNK * 36

namespace minecraft {

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

    std::array< glm::ivec2, ChunkNeighbour_Count > get_chunk_neighbour_directions();

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

    struct Sub_Chunk_Vertex
    {
        u32 packed_vertex_attributes0;
        u32 packed_vertex_attributes1;
    };

    struct Sub_Chunk_Instance
    {
        glm::ivec2 chunk_coords;
    };

    struct Sub_Chunk_Bucket
    {
        i32               memory_id;
        Sub_Chunk_Vertex *current_vertex;
        i32               face_count;
    };

    bool initialize_sub_chunk_bucket(Sub_Chunk_Bucket *sub_chunk_bucket);
    bool is_sub_chunk_bucket_allocated(const Sub_Chunk_Bucket *sub_chunk_bucket);

    struct Sub_Chunk_Render_Data
    {
        i32                 instance_memory_id;
        Sub_Chunk_Instance *base_instance;

        AABB aabb[2];

        std::atomic< i32 > bucket_index;
        Sub_Chunk_Bucket opaque_buckets[2];
        Sub_Chunk_Bucket transparent_buckets[2];

        std::atomic< bool > pending_for_tessellation;
        std::atomic< bool > tessellated;

        i32 face_count;
    };

    struct Chunk
    {
        glm::ivec2 world_coords;
        glm::vec3  position;

        Chunk *neighbours[ChunkNeighbour_Count];

        std::atomic< ChunkState > state;
        std::atomic< bool > pending_for_tessellation;
        std::atomic< bool > tessellated;

        Block blocks[CHUNK_HEIGHT * CHUNK_DEPTH * CHUNK_WIDTH];
        Block front_edge_blocks[CHUNK_HEIGHT * CHUNK_WIDTH];
        Block back_edge_blocks[CHUNK_HEIGHT  * CHUNK_WIDTH];
        Block left_edge_blocks[CHUNK_HEIGHT  * CHUNK_DEPTH];
        Block right_edge_blocks[CHUNK_HEIGHT * CHUNK_DEPTH];

        Block_Light_Info light_map[CHUNK_HEIGHT * CHUNK_DEPTH * CHUNK_WIDTH];
        Block_Light_Info front_edge_light_map[CHUNK_HEIGHT * CHUNK_WIDTH];
        Block_Light_Info back_edge_light_map[CHUNK_HEIGHT  * CHUNK_WIDTH];
        Block_Light_Info left_edge_light_map[CHUNK_HEIGHT  * CHUNK_DEPTH];
        Block_Light_Info right_edge_light_map[CHUNK_HEIGHT * CHUNK_DEPTH];

        Sub_Chunk_Render_Data sub_chunks_render_data[CHUNK_HEIGHT / SUB_CHUNK_HEIGHT];
    };

    bool initialize_chunk(Chunk *chunk,
                          const glm::ivec2 &world_coords);

    void generate_chunk(Chunk *chunk, i32 seed);

    void propagate_sky_light(World *world,
                             Chunk *chunk,
                             Circular_FIFO_Queue< struct Block_Query_Result > *queue);

    void calculate_lighting(World *world,
                            Chunk *chunk,
                            Circular_FIFO_Queue< struct Block_Query_Result > *queue);

    void serialize_chunk(World *world,
                         Chunk *chunk,
                         i32 seed,
                         Temprary_Memory_Arena *temp_arena);

    void deserialize_chunk(World *world,
                           Chunk *chunk,
                           Temprary_Memory_Arena *temp_arena);

    i32 get_block_index(const glm::ivec3& block_coords);
    glm::vec3 get_block_position(Chunk *chunk, const glm::ivec3& block_coords);
    Block* get_block(Chunk *chunk, const glm::ivec3& block_coords);
    Block_Light_Info* get_block_light_info(Chunk *chunk, const glm::ivec3& block_coords);
    glm::ivec2 world_position_to_chunk_coords(const glm::vec3& position);

    Block* get_neighbour_block_from_right(Chunk  *chunk, const glm::ivec3& block_coords);
    Block* get_neighbour_block_from_left(Chunk   *chunk, const glm::ivec3& block_coords);
    Block* get_neighbour_block_from_top(Chunk    *chunk, const glm::ivec3& block_coords);
    Block* get_neighbour_block_from_bottom(Chunk *chunk, const glm::ivec3& block_coords);
    Block* get_neighbour_block_from_front(Chunk  *chunk, const glm::ivec3& block_coords);
    Block* get_neighbour_block_from_back(Chunk   *chunk, const glm::ivec3& block_coords);
    std::array< Block*, 6 > get_neighbours(Chunk *chunk, const glm::ivec3& block_coords);

    struct Block_Query_Result
    {
        glm::ivec3  block_coords;
        Block      *block;
        Chunk      *chunk;
    };

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

    #define INVALID_CHUNK_ENTRY -1

    struct Chunk_Hash_Table_Entry
    {
        glm::ivec2 chunk_coords;
        i16        chunk_node_index;
    };

    struct World
    {
        static constexpr i64 sub_chunk_height          = SUB_CHUNK_HEIGHT;
        static constexpr i64 sub_chunk_count_per_chunk = CHUNK_HEIGHT / sub_chunk_height;
        static_assert(CHUNK_HEIGHT % sub_chunk_height == 0);

        static constexpr i64 max_chunk_radius              = 30;
        static constexpr i64 pending_free_chunk_radius     = 2;
        static constexpr i64 chunk_capacity                = 4 * (max_chunk_radius + pending_free_chunk_radius) * (max_chunk_radius + pending_free_chunk_radius);

        static constexpr i64 sub_chunk_bucket_capacity     = 4 * chunk_capacity;
        static constexpr i64 sub_chunk_bucket_face_count   = 1024;
        static constexpr i64 sub_chunk_bucket_vertex_count = 4 * sub_chunk_bucket_face_count;
        static constexpr i64 sub_chunk_bucket_size         = sub_chunk_bucket_vertex_count * sizeof(Sub_Chunk_Vertex);

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
        Chunk_Node  chunk_nodes[chunk_capacity];
        Chunk_Node *first_free_chunk_node;

        static constexpr i64   chunk_hash_table_capacity = chunk_capacity;
        Chunk_Hash_Table_Entry chunk_hash_table[chunk_hash_table_capacity];

        Update_Chunk_Job                        update_chunk_jobs_queue_data[DEFAULT_QUEUE_SIZE];
        Circular_FIFO_Queue< Update_Chunk_Job > update_chunk_jobs_queue;

        Calculate_Chunk_Light_Propagation_Job                        light_propagation_queue_data[DEFAULT_QUEUE_SIZE];
        Circular_FIFO_Queue< Calculate_Chunk_Light_Propagation_Job > light_propagation_queue;

        Calculate_Chunk_Lighting_Job                        calculate_chunk_lighting_queue_data[DEFAULT_QUEUE_SIZE];
        Circular_FIFO_Queue< Calculate_Chunk_Lighting_Job > calculate_chunk_lighting_queue;
    };

    bool initialize_world(World *world, String8 path, Temprary_Memory_Arena *temp_arena);
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

    // todo(harlequin): hash function from https://carmencincotti.com/2022-10-31/spatial-hash-maps-part-one/
    inline i64 get_chunk_hash(glm::ivec2 coords)
    {
        // return glm::abs((i64)coords.x | ((i64)coords.y << (i64)32)) % World::chunk_hash_table_capacity;
        return glm::abs(((i64)coords.x * (i64)92837111) ^ ((i64)coords.y * (i64)689287499)) % World::chunk_hash_table_capacity;
    }

    bool insert_chunk(World *world, Chunk *chunk);
    bool remove_chunk(World *world, glm::ivec2 coords);
    Chunk *get_chunk(World *world, glm::ivec2 coords);

    void load_chunks_at_region(World *world, const World_Region_Bounds& region_bounds);
    void free_chunks_out_of_region(World *world, const World_Region_Bounds& region_bounds);

    bool is_chunk_in_region_bounds(const glm::ivec2& chunk_coords, const World_Region_Bounds& region_bounds);

    glm::ivec3 world_position_to_block_coords(World *world, const glm::vec3& position);
    World_Region_Bounds get_world_bounds_from_chunk_coords(i32 chunk_radius, const glm::ivec2& chunk_coords);

    Block* get_block(World *world, const glm::vec3& position);
    Block_Query_Result query_block(World *world, const glm::vec3& position);

    bool is_block_query_valid(const Block_Query_Result& query);

    bool is_block_query_in_world_region(const Block_Query_Result& query, const World_Region_Bounds& bounds);

    i32 get_sub_chunk_index(const glm::ivec3& block_coords);

    Block_Query_Result query_neighbour_block_from_top(Chunk *chunk,    const glm::ivec3& block_coords);
    Block_Query_Result query_neighbour_block_from_bottom(Chunk *chunk, const glm::ivec3& block_coords);

    Block_Query_Result query_neighbour_block_from_left(Chunk *chunk,  const glm::ivec3& block_coords);
    Block_Query_Result query_neighbour_block_from_right(Chunk *chunk, const glm::ivec3& block_coords);

    Block_Query_Result query_neighbour_block_from_front(Chunk *chunk, const glm::ivec3& block_coords);
    Block_Query_Result query_neighbour_block_from_back(Chunk *chunk,  const glm::ivec3& block_coords);

    std::array< Block_Query_Result, 6 > query_neighbours(Chunk *chunk, const glm::ivec3& block_coords);

    String8 get_chunk_file_path(World *world, Chunk *chunk, Temprary_Memory_Arena *temp_arena);

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

    Select_Block_Result select_block(World          *world,
                                    const glm::vec3 &view_position,
                                    const glm::vec3 &view_direction,
                                    u32              max_block_select_dist_in_cube_units);

    void schedule_chunk_lighting_jobs(World *world,
                                      const World_Region_Bounds &player_region_bounds);
    void schedule_save_chunks_jobs(World *world);

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
}