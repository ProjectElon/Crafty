#pragma once

#include "core/common.h"

#include "meta/spritesheet_meta.h"
#include "memory/memory_arena.h"
#include "game/math.h"
#include "game/jobs.h"
#include "game/console_commands.h"
#include "containers/string.h"
#include "containers/robin_hood.h"
#include "containers/queue.h"

#include <glm/glm.hpp>

#include <mutex>
#include <algorithm>
#include <array>
#include <queue>
#include <atomic>

#define MC_CHUNK_HEIGHT 256
#define MC_CHUNK_DEPTH  16
#define MC_CHUNK_WIDTH  16

#define MC_BLOCK_COUNT_PER_CHUNK MC_CHUNK_WIDTH * MC_CHUNK_HEIGHT * MC_CHUNK_DEPTH
#define MC_INDEX_COUNT_PER_CHUNK MC_BLOCK_COUNT_PER_CHUNK * 36

#define MC_SUB_CHUNK_HEIGHT 8
#define MC_BLOCK_COUNT_PER_SUB_CHUNK  MC_CHUNK_WIDTH * MC_CHUNK_DEPTH * MC_SUB_CHUNK_HEIGHT
#define MC_VERTEX_COUNT_PER_SUB_CHUNK MC_BLOCK_COUNT_PER_SUB_CHUNK * 24
#define MC_INDEX_COUNT_PER_SUB_CHUNK  MC_BLOCK_COUNT_PER_SUB_CHUNK * 36

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
        BlockFlags_Should_Color_Bottom_By_Biome = 16,
        BlockFlags_Is_Light_Source = 32
    };

    struct Block_Info
    {
        const char *name;
        u16         top_texture_id;
        u16         bottom_texture_id;
        u16         side_texture_id;
        u32         flags;
    };

    inline bool is_block_solid(const Block_Info *block_info)
    {
        return block_info->flags & BlockFlags_Is_Solid;
    }

    inline bool is_block_transparent(const Block_Info *block_info)
    {
        return block_info->flags & BlockFlags_Is_Transparent;
    }

    inline bool is_light_source(const Block_Info *block_info)
    {
        return block_info->flags & BlockFlags_Is_Light_Source;
    }

    inline bool should_color_top_by_biome(const Block_Info *block_info)
    {
        return block_info->flags & BlockFlags_Should_Color_Top_By_Biome;
    }

    inline bool should_color_side_by_biome(const Block_Info *block_info)
    {
        return block_info->flags & BlockFlags_Should_Color_Side_By_Biome;
    }

    inline bool should_color_bottom_by_biome(const Block_Info *block_info)
    {
        return block_info->flags & BlockFlags_Should_Color_Bottom_By_Biome;
    }

    struct Block
    {
        u16 id;
    };

    struct Block_Light_Info
    {
        u8 sky_light_level;
        u8 light_source_level;
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

    struct Sub_Chunk_Bucket
    {
        i32               memory_id;
        Sub_Chunk_Vertex *current_vertex;
        i32               face_count;
    };

    inline bool is_sub_chunk_bucket_allocated(const Sub_Chunk_Bucket* sub_chunk_bucket)
    {
        return sub_chunk_bucket->memory_id != -1 && sub_chunk_bucket->current_vertex != nullptr;
    }

    struct Sub_Chunk_Render_Data
    {
        std::atomic<i32> bucket_index;
        Sub_Chunk_Bucket opaque_buckets[2];
        Sub_Chunk_Bucket transparent_buckets[2];

        i32                 instance_memory_id;
        Sub_Chunk_Instance *base_instance;

        AABB aabb[2];

        std::atomic<bool> uploaded_to_gpu;
        std::atomic<bool> pending_for_update;

        i32 face_count;
    };

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

    struct Chunk
    {
        glm::ivec2  world_coords;
        glm::vec3   position;
        char        file_path[256];

        Chunk*      neighbours[ChunkNeighbour_Count];

        std::atomic<bool> pending_for_load;
        std::atomic<bool> loaded;
        std::atomic<bool> neighbours_loaded;
        std::atomic<bool> pending_for_lighting;
        std::atomic<bool> in_light_propagation_queue;
        std::atomic<bool> in_light_calculation_queue;
        std::atomic<bool> light_propagated;
        std::atomic<bool> light_calculated;
        std::atomic<bool> pending_for_update;
        std::atomic<bool> pending_for_save;
        std::atomic<bool> unload;

        Block blocks[MC_CHUNK_HEIGHT * MC_CHUNK_DEPTH * MC_CHUNK_WIDTH];
        Block front_edge_blocks[MC_CHUNK_HEIGHT * MC_CHUNK_WIDTH];
        Block back_edge_blocks[MC_CHUNK_HEIGHT  * MC_CHUNK_WIDTH];
        Block left_edge_blocks[MC_CHUNK_HEIGHT  * MC_CHUNK_DEPTH];
        Block right_edge_blocks[MC_CHUNK_HEIGHT * MC_CHUNK_DEPTH];

        Block_Light_Info light_map[MC_CHUNK_HEIGHT * MC_CHUNK_DEPTH * MC_CHUNK_WIDTH];
        Block_Light_Info front_edge_light_map[MC_CHUNK_HEIGHT * MC_CHUNK_WIDTH];
        Block_Light_Info back_edge_light_map[MC_CHUNK_HEIGHT  * MC_CHUNK_WIDTH];
        Block_Light_Info left_edge_light_map[MC_CHUNK_HEIGHT  * MC_CHUNK_DEPTH];
        Block_Light_Info right_edge_light_map[MC_CHUNK_HEIGHT * MC_CHUNK_DEPTH];

        Sub_Chunk_Render_Data sub_chunks_render_data[MC_CHUNK_HEIGHT / MC_SUB_CHUNK_HEIGHT];
    };

    bool initialize_chunk(Chunk *chunk, const glm::ivec2 &world_coords, String8 world_path);
    void generate_chunk(Chunk *chunk, i32 seed);

    void propagate_sky_light(World *world, Chunk *chunk, Circular_FIFO_Queue<struct Block_Query_Result> *queue);
    void calculate_lighting(World *world, Chunk *chunk, Circular_FIFO_Queue<struct Block_Query_Result> *queue);

    void serialize_chunk(Chunk *chunk, i32 seed, String8 world_path, Temprary_Memory_Arena *temp_arena);
    void deserialize_chunk(Chunk *chunk);

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
    std::array<Block*, 6> get_neighbours(Chunk   *chunk, const glm::ivec3& block_coords);

    struct Chunk_Hash
    {
        std::size_t operator()(const glm::ivec2& coord) const
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

    struct World_Region_Bounds
    {
        glm::ivec2 min;
        glm::ivec2 max;
    };

    struct Ray_Cast_Result;

    struct Chunk_Node
    {
        Chunk       chunk;
        Chunk_Node *next;
    };

    #define INVALID_CHUNK_ENTRY -1

    struct chunk_entry
    {
        glm::ivec2 coords;
        i16        index;
    };

    struct World
    {
        static constexpr i64 sub_chunk_height          = MC_SUB_CHUNK_HEIGHT;
        static constexpr i64 sub_chunk_count_per_chunk = MC_CHUNK_HEIGHT / sub_chunk_height;
        static_assert(MC_CHUNK_HEIGHT % sub_chunk_height == 0);

        static constexpr i64 max_chunk_radius              = 30;
        static constexpr i64 pending_free_chunk_radius     = 2;
        static constexpr i64 chunk_capacity                = 4 * (max_chunk_radius + pending_free_chunk_radius) * (max_chunk_radius + pending_free_chunk_radius);

        static constexpr i64 sub_chunk_bucket_capacity     = 4 * chunk_capacity;
        static constexpr i64 sub_chunk_bucket_face_count   = 1024;
        static constexpr i64 sub_chunk_bucket_vertex_count = 4 * sub_chunk_bucket_face_count;
        static constexpr i64 sub_chunk_bucket_size         = sub_chunk_bucket_vertex_count * sizeof(Sub_Chunk_Vertex);

        // todo(harlequin): to be added to game_config
        i32                 chunk_radius;
        f32                 sky_light_level;

        String8             path;
        i32                 seed;
        World_Region_Bounds player_region_bounds;

        static Block            null_block;
        static const Block_Info block_infos[BlockId_Count];  // todo(harlequin): this is going to be content driven in the future with the help of a tool

        // robin_hood::unordered_node_map< glm::ivec2, Chunk*, Chunk_Hash > loaded_chunks; // @todo(harlequin): to be removed

        u32         free_chunk_count;
        Chunk_Node  chunk_nodes[chunk_capacity];
        Chunk_Node *free_chunk_list_head;

        static constexpr i64 chunk_hash_table_capacity = chunk_capacity;
        chunk_entry chunk_hash_table[chunk_hash_table_capacity];

        std::vector< Chunk* >                                      pending_free_chunks; // @todo(harlequin): to be removed

        Circular_FIFO_Queue<Update_Chunk_Job>                      update_chunk_jobs_queue;
        Circular_FIFO_Queue<Calculate_Chunk_Light_Propagation_Job> light_propagation_queue;
        Circular_FIFO_Queue<Calculate_Chunk_Lighting_Job>          calculate_chunk_lighting_queue;
    };

    bool initialize_world(World *world, String8 path);
    void shutdown_world(World *world);

    Chunk *allocate_chunk(World *world);
    void free_chunk(World *world, Chunk *chunk);

    inline u64 get_chunk_hash(glm::ivec2 coords)
    {
        return ((u64)coords.x | ((u64)coords.y << (u64)32)) % World::chunk_hash_table_capacity;
    }

    bool insert_chunk(World *world, Chunk *chunk);
    bool remove_chunk(World *world, glm::ivec2 coords);
    Chunk *get_chunk(World *world, glm::ivec2 coords);

    void load_chunks_at_region(World *world, const World_Region_Bounds& region_bounds);
    void free_chunks_out_of_region(World *world);

    bool is_chunk_in_region_bounds(const glm::ivec2& chunk_coords, const World_Region_Bounds& region_bounds);

    glm::ivec3 world_position_to_block_coords(World *world, const glm::vec3& position);
    World_Region_Bounds get_world_bounds_from_chunk_coords(i32 chunk_radius, const glm::ivec2& chunk_coords);

    // Chunk* get_chunk(World *world, const glm::ivec2& chunk_coords);

    Block* get_block(World *world, const glm::vec3& position);
    Block_Query_Result query_block(World *world, const glm::vec3& position);

    bool is_block_query_valid(const Block_Query_Result& query);

    bool is_block_query_in_world_region(const Block_Query_Result& query, const World_Region_Bounds& bounds);

    i32 get_sub_chunk_index(const glm::ivec3& block_coords);

    Block_Query_Result world_get_neighbour_block_from_top(Chunk *chunk, const glm::ivec3& block_coords);
    Block_Query_Result world_get_neighbour_block_from_bottom(Chunk *chunk, const glm::ivec3& block_coords);

    Block_Query_Result world_get_neighbour_block_from_left(Chunk *chunk, const glm::ivec3& block_coords);
    Block_Query_Result world_get_neighbour_block_from_right(Chunk *chunk, const glm::ivec3& block_coords);

    Block_Query_Result world_get_neighbour_block_from_front(Chunk *chunk, const glm::ivec3& block_coords);
    Block_Query_Result world_get_neighbour_block_from_back(Chunk *chunk, const glm::ivec3& block_coords);

    std::array<Block_Query_Result, 6> world_get_neighbours(Chunk *chunk, const glm::ivec3& block_coords);

    Block_Query_Result select_block(World           *world,
                                    const glm::vec3 &view_position,
                                    const glm::vec3 &view_direction,
                                    u32              max_block_select_dist_in_cube_units,
                                    Ray_Cast_Result *out_ray_cast_result = nullptr);

    void schedule_chunk_lighting_jobs(World *world, World_Region_Bounds *player_region_bounds);
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