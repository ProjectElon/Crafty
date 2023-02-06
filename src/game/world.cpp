#include "world.h"
#include "core/file_system.h"
#include "renderer/opengl_renderer.h"
#include "game/job_system.h"
#include "game/jobs.h"
#include "memory/memory_arena.h"
#include <glm/gtx/compatibility.hpp>

#include <time.h>
#include <errno.h>

extern int errno;

namespace minecraft {

    bool initialize_world(World *world, String8 world_path, Temprary_Memory_Arena *temp_arena)
    {
        namespace fs = std::filesystem;

        if (!File_System::exists(world_path.data))
        {
            File_System::create_directory(world_path.data);
        }

        world->path = world_path;

        String8 meta_file_path = push_string8(temp_arena,
                                              "%.*s/meta",
                                              (i32)world_path.count,
                                              world_path.data);

        FILE* meta_file = fopen(meta_file_path.data, "rb");

        if (meta_file)
        {
            fscanf(meta_file, "%d", &world->seed);
        }
        else
        {
            srand((u32)time(nullptr));
            world->seed = (i32)(((f32)rand() / (f32)RAND_MAX) * 1000000.0f);
            meta_file = fopen(meta_file_path.data, "wb");
            fprintf(meta_file, "%d", world->seed);
        }

        fclose(meta_file);

        for (u32 i = 0; i < World::ChunkCapacity; i++)
        {
            set_entry_state(world->chunk_hash_table_values[i], ChunkHashTableEntryState_Empty);
        }

        Chunk_Node *last_chunk_node = &world->chunk_nodes[World::ChunkCapacity - 1];
        last_chunk_node->next       = nullptr;

        for (u32 i = 0; i < World::ChunkCapacity - 1; i++)
        {
            Chunk_Node *chunk_node = world->chunk_nodes + i;
            chunk_node->next = world->chunk_nodes + i + 1;
        }

        world->free_chunk_count      = World::ChunkCapacity;
        world->first_free_chunk_node = &world->chunk_nodes[0];

        world->update_chunk_jobs_queue.initialize();
        world->calculate_chunk_lighting_queue.initialize();
        world->light_propagation_queue.initialize();

        world->game_timer     = 0.0f;
        world->game_time_rate = 1.0f / 72.0f; // 1 / 72.0f is the number used by minecraft
        world->game_time      = 43200;

        return true;
    }

    void shutdown_world(World *world)
    {
    }

    void update_world_time(World *world, f32 delta_time)
    {
        world->game_timer += delta_time;

        while (world->game_timer >= world->game_time_rate)
        {
            world->game_time++;

            if (world->game_time > 86400)
            {
                world->game_time -= 86400;
            }

            world->game_timer -= world->game_time_rate;
        }

        // night time
        if (world->game_time >= 0 && world->game_time <= 43200)
        {
            f32 t = (f32)world->game_time / 43200.0f;
            world->sky_light_level = glm::ceil(glm::lerp(1.0f, 15.0f, t));
        } // day time
        else if (world->game_time >= 43200 && world->game_time <= 86400)
        {
            f32 t = ((f32)world->game_time - 43200.0f) / 43200.0f;
            world->sky_light_level = glm::ceil(glm::lerp(15.0f, 1.0f, t));
        }
    }

    u32 real_time_to_game_time(u32 hours, u32 minutes, u32 seconds)
    {
        return seconds + minutes * 60 + hours * 60 * 60;
    }

    void game_time_to_real_time(u32  game_time,
                                u32 *out_hours,
                                u32 *out_minutes,
                                u32 *out_seconds)
    {
        *out_seconds = game_time % 60;
        *out_minutes = (game_time % (60 * 60)) / 60;
        *out_hours   = game_time / (60 * 60);
    }

    Chunk *allocate_chunk(World *world)
    {
        Assert(world->free_chunk_count);
        world->free_chunk_count--;
        Chunk_Node *chunk_node       = world->first_free_chunk_node;
        world->first_free_chunk_node = world->first_free_chunk_node->next;
        return &chunk_node->chunk;
    }

    void free_chunk(World *world, Chunk *chunk)
    {
        Assert(chunk);
        Chunk_Node *chunk_node = (Chunk_Node*)chunk;
        Assert(chunk_node >= world->chunk_nodes && chunk_node <= world->chunk_nodes + World::ChunkCapacity);
        world->free_chunk_count++;
        chunk_node->next             = world->first_free_chunk_node;
        world->first_free_chunk_node = chunk_node;
    }

    static inline u16 get_chunk_node_index(World *world, Chunk *chunk)
    {
        Chunk_Node *chunk_node = (Chunk_Node*)chunk;
        Assert(chunk_node >= world->chunk_nodes && chunk_node <= world->chunk_nodes + World::ChunkCapacity);
        return (u16)(chunk_node - world->chunk_nodes);
    }

    Chunk* insert_and_allocate_chunk(World            *world,
                                     const glm::ivec2 &chunk_coords)
    {
        u32  start_index        = (u32)(get_chunk_hash(chunk_coords) % World::ChunkHashTableCapacity);
        u32  index              = start_index;
        i32  insertion_index    = -1;
        bool found              = false;

        do
        {
            u16 &entry = world->chunk_hash_table_values[index];
            ChunkHashTableEntryState state = get_entry_state(entry);

            if (state == ChunkHashTableEntryState_Empty)
            {
                bool is_insertion_index_set = insertion_index != -1;
                if (!is_insertion_index_set)
                {
                    insertion_index = index;
                }
                break;
            }
            else if (state == ChunkHashTableEntryState_Deleted)
            {
                bool is_insertion_index_set = insertion_index != -1;
                if (!is_insertion_index_set)
                {
                    insertion_index = index;
                }
            }
            else if (world->chunk_hash_table_keys[index] == chunk_coords)
            {
                found = true;
                break;
            }

            index++;
            if (index == World::ChunkHashTableCapacity)
            {
                index = 0;
            }
        }
        while (index != start_index);

        Chunk *chunk = nullptr;

        if (!found)
        {
            chunk = allocate_chunk(world);
            u16 chunk_node_index = get_chunk_node_index(world, chunk);
            u16 &entry = world->chunk_hash_table_values[insertion_index];
            set_entry_state(entry, ChunkHashTableEntryState_Occupied);
            set_entry_value(entry, chunk_node_index);
            world->chunk_hash_table_keys[insertion_index] = chunk_coords;
        }

        return chunk;
    }

    Chunk* get_chunk(World *world, const glm::ivec2& coords)
    {
        u32  start_index = (u32)(get_chunk_hash(coords) % World::ChunkHashTableCapacity);
        u32  index       = start_index;

        do
        {
            u16 &entry = world->chunk_hash_table_values[index];
            ChunkHashTableEntryState state = get_entry_state(entry);
            if (state == ChunkHashTableEntryState_Empty)
            {
                break;
            }
            else if (state == ChunkHashTableEntryState_Occupied &&
                     world->chunk_hash_table_keys[index] == coords)
            {
                u16 chunk_node_index = get_entry_value(entry);
                return &world->chunk_nodes[chunk_node_index].chunk;
            }

            index++;
            if (index == World::ChunkHashTableCapacity)
            {
                index = 0;
            }
        }
        while (index != start_index);

        return nullptr;
    }

    bool remove_chunk(World *world, const glm::ivec2& coords)
    {
        u32  start_index        = (u32)(get_chunk_hash(coords) % World::ChunkHashTableCapacity);
        u32  index              = start_index;

        do
        {
            u16 &entry = world->chunk_hash_table_values[index];
            ChunkHashTableEntryState state = get_entry_state(entry);
            if (state == ChunkHashTableEntryState_Empty)
            {
                break;
            }
            else if (state == ChunkHashTableEntryState_Occupied &&
                     world->chunk_hash_table_keys[index] == coords)
            {
                set_entry_state(entry, ChunkHashTableEntryState_Deleted);
                return true;
            }

            index++;
            if (index == World::ChunkHashTableCapacity)
            {
                index = 0;
            }
        }
        while (index != start_index);

        return false;
    }

    void load_and_update_chunks(World *world, const World_Region_Bounds& region_bounds)
    {
        for (i32 z = region_bounds.min.y - World::PendingFreeChunkRadius;
                 z <= region_bounds.max.y + World::PendingFreeChunkRadius; ++z)
        {
            for (i32 x = region_bounds.min.x - World::PendingFreeChunkRadius;
                     x <= region_bounds.max.x + World::PendingFreeChunkRadius; ++x)
            {
                glm::ivec2 chunk_coords = { x, z };

                Chunk *chunk = insert_and_allocate_chunk(world, chunk_coords);
                if (chunk)
                {
                    initialize_chunk(chunk, chunk_coords);
                    Load_Chunk_Job load_chunk_job = {};
                    load_chunk_job.world = world;
                    load_chunk_job.chunk = chunk;
                    Job_System::schedule(load_chunk_job);
                }
            }
        }

        for (u32 i = 0; i < World::ChunkHashTableCapacity; i++)
        {
            if (get_entry_state(world->chunk_hash_table_values[i]) != ChunkHashTableEntryState_Occupied)
            {
                continue;
            }

            const glm::ivec2& chunk_coords = world->chunk_hash_table_keys[i];

            u16 chunk_node_index = get_entry_value(world->chunk_hash_table_values[i]);
            Chunk *chunk = &world->chunk_nodes[chunk_node_index].chunk;
            Assert(chunk);

            if (is_chunk_in_region_bounds(chunk_coords, world->active_region_bounds))
            {
                bool all_neighbours_loaded = true;

                for (i32 i = 0; i < ChunkNeighbour_Count; i++)
                {
                    glm::ivec2 neighbour_chunk_coords = chunk_coords + Chunk::NeighbourDirections[i];

                    Chunk *neighbour_chunk = chunk->neighbours[i];

                    if (!neighbour_chunk)
                    {
                        neighbour_chunk = get_chunk(world, neighbour_chunk_coords);
                        Assert(neighbour_chunk);
                        chunk->neighbours[i] = neighbour_chunk;
                    }

                    if (neighbour_chunk->state == ChunkState_Initialized)
                    {
                        all_neighbours_loaded = false;
                    }
                }

                if (all_neighbours_loaded && chunk->state == ChunkState_Loaded)
                {
                    chunk->state = ChunkState_NeighboursLoaded;
                }

                auto& light_propagation_queue        = world->light_propagation_queue;
                auto& calculate_chunk_lighting_queue = world->calculate_chunk_lighting_queue;

                if (chunk->state == ChunkState_NeighboursLoaded)
                {
                    chunk->state = ChunkState_PendingForLightPropagation;

                    Calculate_Chunk_Light_Propagation_Job job;
                    job.world = world;
                    job.chunk = chunk;
                    light_propagation_queue.push(job);
                }
                else if (chunk->state == ChunkState_LightPropagated)
                {
                    bool all_neighbours_light_propagated = true;

                    for (i32 i = 0; i < ChunkNeighbour_Count; i++)
                    {
                        Chunk *neighbour_chunk = chunk->neighbours[i];

                        if (neighbour_chunk->state < ChunkState_LightPropagated)
                        {
                            all_neighbours_light_propagated = false;
                            break;
                        }
                    }

                    if (all_neighbours_light_propagated)
                    {
                        chunk->state = ChunkState_PendingForLightCalculation;

                        Calculate_Chunk_Lighting_Job job;
                        job.world = world;
                        job.chunk = chunk;
                        calculate_chunk_lighting_queue.push(job);
                    }
                }
            }

            bool out_of_bounds = chunk_coords.x < region_bounds.min.x - World::PendingFreeChunkRadius ||
                                 chunk_coords.x > region_bounds.max.x + World::PendingFreeChunkRadius ||
                                 chunk_coords.y < region_bounds.min.y - World::PendingFreeChunkRadius ||
                                 chunk_coords.y > region_bounds.max.y + World::PendingFreeChunkRadius;

            if (out_of_bounds &&
                chunk->tessellation_state != TessellationState_Pending &&
                (chunk->state == ChunkState_Loaded ||
                 chunk->state == ChunkState_NeighboursLoaded ||
                 chunk->state == ChunkState_LightCalculated))
            {
                chunk->state = ChunkState_PendingForSave;

                Serialize_And_Free_Chunk_Job serialize_and_free_chunk_job;
                serialize_and_free_chunk_job.world = world;
                serialize_and_free_chunk_job.chunk = chunk;
                bool is_high_priority = false;
                Job_System::schedule(serialize_and_free_chunk_job, is_high_priority);

                bool removed = remove_chunk(world, chunk->world_coords);
                Assert(removed);
            }
        }

        for (u32 i = 0; i < World::ChunkCapacity; i++)
        {
            Chunk* chunk = &world->chunk_nodes[i].chunk;
            if (chunk->state == ChunkState_Saved)
            {
                chunk->state = ChunkState_Freed;
                free_chunk(world, chunk);
            }
        }
    }

    glm::ivec3 world_position_to_block_coords(World *world, const glm::vec3 &position)
    {
        glm::ivec2 chunk_coords  = world_position_to_chunk_coords(position);
        glm::vec3 chunk_position = { chunk_coords.x * Chunk::Width, 0.0f, chunk_coords.y * Chunk::Depth };
        glm::vec3 offset         = position - chunk_position;
        return { (i32)glm::floor(offset.x), (i32)glm::floor(position.y), (i32)glm::floor(offset.z) };
    }

    World_Region_Bounds get_world_bounds_from_chunk_coords(i32 chunk_radius, const glm::ivec2 &chunk_coords)
    {
        World_Region_Bounds bounds;
        bounds.min = chunk_coords - glm::ivec2(chunk_radius, chunk_radius);
        bounds.max = chunk_coords + glm::ivec2(chunk_radius, chunk_radius);
        return bounds;
    }

    bool is_chunk_in_region_bounds(const glm::ivec2 &chunk_coords,
                                    const World_Region_Bounds &region_bounds)
    {
        return chunk_coords.x >= region_bounds.min.x && chunk_coords.x <= region_bounds.max.x &&
               chunk_coords.y >= region_bounds.min.y && chunk_coords.y <= region_bounds.max.y;
    }

    bool is_block_query_valid(const Block_Query_Result &query)
    {
        return query.chunk && query.block &&
               query.block_coords.x >= 0 && query.block_coords.x < Chunk::Width  &&
               query.block_coords.y >= 0 && query.block_coords.y < Chunk::Height &&
               query.block_coords.z >= 0 && query.block_coords.z < Chunk::Depth;
    }

    bool is_block_query_in_world_region(const Block_Query_Result& query, const World_Region_Bounds& bounds)
    {
        return query.chunk->world_coords.x >= bounds.min.x &&
               query.chunk->world_coords.x <= bounds.max.x &&
               query.chunk->world_coords.y >= bounds.min.y &&
               query.chunk->world_coords.y <= bounds.max.y;
    }

    Select_Block_Result select_block(World          *world,
                                    const glm::vec3& view_position,
                                    const glm::vec3& view_direction,
                                    u32              max_block_select_dist_in_cube_units)
    {
        Select_Block_Result result = {};
        result.block_query.block_coords = { -1, -1, -1 };

        glm::vec3 query_position = view_position;

        for (u32 i = 0; i < max_block_select_dist_in_cube_units * 10; i++)
        {
            Block_Query_Result query = query_block(world, query_position);

            if (is_block_query_valid(query) && query.block->id != BlockId_Air && query.block->id != BlockId_Water)
            {
                glm::vec3 block_position = get_block_position(query.chunk, query.block_coords);
                Ray  ray  = { view_position, view_direction };
                AABB aabb = { block_position - glm::vec3(0.5f, 0.5f, 0.5f), block_position + glm::vec3(0.5f, 0.5f, 0.5f) };
                result.ray_cast_result = cast_ray_on_aabb(ray, aabb, (f32)max_block_select_dist_in_cube_units);

                if (result.ray_cast_result.hit)
                {
                    result.block_query = query;
                    break;
                }
            }

            query_position += view_direction * 0.1f;
        }

        if (!result.ray_cast_result.hit)
        {
            return result;
        }

        constexpr f32 eps = glm::epsilon< f32 >();

        const glm::vec3& hit_point = result.ray_cast_result.point;

        result.block_position = get_block_position(result.block_query.chunk,
                                                   result.block_query.block_coords);

        if (glm::epsilonEqual(hit_point.y, result.block_position.y + 0.5f, eps))
        {
            result.block_facing_normal_query = query_neighbour_block_from_top(result.block_query.chunk,
                                                                              result.block_query.block_coords);
            result.normal = { 0.0f, 1.0f, 0.0f };
            result.face   = BlockFace_Top;
        }
        else if (glm::epsilonEqual(hit_point.y, result.block_position.y - 0.5f, eps))
        {
            result.block_facing_normal_query = query_neighbour_block_from_bottom(result.block_query.chunk,
                                                                                 result.block_query.block_coords);
            result.normal = { 0.0f, -1.0f, 0.0f };
            result.face   = BlockFace_Bottom;
        }
        else if (glm::epsilonEqual(hit_point.x, result.block_position.x + 0.5f, eps))
        {
            result.block_facing_normal_query = query_neighbour_block_from_right(result.block_query.chunk,
                                                                                result.block_query.block_coords);
            result.normal = { 1.0f, 0.0f, 0.0f };
            result.face   = BlockFace_Right;
        }
        else if (glm::epsilonEqual(hit_point.x, result.block_position.x - 0.5f, eps))
        {
            result.block_facing_normal_query = query_neighbour_block_from_left(result.block_query.chunk,
                                                                               result.block_query.block_coords);
            result.normal = { -1.0f, 0.0f, 0.0f };
            result.face   = BlockFace_Left;
        }
        else if (glm::epsilonEqual(hit_point.z, result.block_position.z - 0.5f, eps))
        {
            result.block_facing_normal_query = query_neighbour_block_from_front(result.block_query.chunk,
                                                                                result.block_query.block_coords);
            result.normal = { 0.0f, 0.0f, -1.0f };
            result.face   = BlockFace_Front;
        }
        else if (glm::epsilonEqual(hit_point.z, result.block_position.z + 0.5f, eps))
        {
            result.block_facing_normal_query = query_neighbour_block_from_back(result.block_query.chunk,
                                                                               result.block_query.block_coords);
            result.normal = { 0.0f, 0.0f, 1.0f };
            result.face   = BlockFace_Back;
        }

        if (is_block_query_valid(result.block_facing_normal_query))
        {
            result.block_facing_normal_position = get_block_position(result.block_facing_normal_query.chunk,
                                                                     result.block_facing_normal_query.block_coords);

        }

        return result;
    }

    void free_chunks_out_of_region(World *world, const World_Region_Bounds& region_bounds)
    {
        for (u32 i = 0; i < World::ChunkHashTableCapacity; i++)
        {
            if (get_entry_state(world->chunk_hash_table_values[i]) != ChunkHashTableEntryState_Occupied)
            {
                continue;
            }

            const glm::ivec2& chunk_coords = world->chunk_hash_table_keys[i];
            u16 chunk_node_index = get_entry_value(world->chunk_hash_table_values[i]);
            Chunk *chunk = &world->chunk_nodes[chunk_node_index].chunk;
        }
    }

    static void queue_update_sub_chunk_job(Circular_Queue< Update_Chunk_Job > &queue, Chunk *chunk, i32 sub_chunk_index)
    {
        Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];

        if (render_data.state != TessellationState_Pending)
        {
            render_data.state = TessellationState_Pending;

            if (chunk->tessellation_state != TessellationState_Pending)
            {
                chunk->tessellation_state = TessellationState_Pending;

                Update_Chunk_Job job;
                job.chunk = chunk;
                queue.push(job);
            }
        }
    }

    void set_block_id(Chunk *chunk, const glm::ivec3& block_coords, u16 block_id)
    {
        chunk->state = ChunkState_NeighboursLoaded;

        for (i32 i = 0; i < ChunkNeighbour_Count; i++)
        {
            Chunk* neighbour = chunk->neighbours[i];
            Assert(neighbour);

            neighbour->state = ChunkState_NeighboursLoaded;
        }

        Block *block = get_block(chunk, block_coords);
        block->id = block_id;

        i32 sub_chunk_index = get_sub_chunk_render_data_index(block_coords);

        if (block_coords.x == 0)
        {
            Chunk *left_chunk = chunk->neighbours[ChunkNeighbour_Left];
            Assert(left_chunk);
            left_chunk->right_edge_blocks[block_coords.y * Chunk::Depth + block_coords.z].id = block_id;
        }
        else if (block_coords.x == Chunk::Width - 1)
        {
            Chunk *right_chunk = chunk->neighbours[ChunkNeighbour_Right];
            Assert(right_chunk);
            right_chunk->left_edge_blocks[block_coords.y * Chunk::Depth + block_coords.z].id = block_id;
        }

        if (block_coords.z == 0)
        {
            Chunk *front_chunk = chunk->neighbours[ChunkNeighbour_Front];
            Assert(front_chunk);
            front_chunk->back_edge_blocks[block_coords.y * Chunk::Width + block_coords.x].id = block_id;
        }
        else if (block_coords.z == Chunk::Depth - 1)
        {
            Chunk *back_chunk = chunk->neighbours[ChunkNeighbour_Back];
            Assert(back_chunk);
            back_chunk->front_edge_blocks[block_coords.y * Chunk::Width + block_coords.x].id = block_id;
        }
    }

    // todo(harlequin): remove *world
    void set_block_sky_light_level(World *world, Chunk *chunk, const glm::ivec3& block_coords, u8 light_level)
    {
        Block *block = get_block(chunk, block_coords);
        Block_Light_Info *light_info = get_block_light_info(chunk, block_coords);
        light_info->sky_light_level = light_level;

        i32 sub_chunk_index = get_sub_chunk_render_data_index(block_coords);
        queue_update_sub_chunk_job(world->update_chunk_jobs_queue, chunk, sub_chunk_index);

        if (block_coords.x == 0)
        {
            Chunk *left_chunk = chunk->neighbours[ChunkNeighbour_Left];
            Assert(left_chunk);
            left_chunk->right_edge_light_map[block_coords.y * Chunk::Depth + block_coords.z].sky_light_level = light_level;
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, left_chunk, sub_chunk_index);
        }
        else if (block_coords.x == Chunk::Width - 1)
        {
            Chunk *right_chunk = chunk->neighbours[ChunkNeighbour_Right];
            Assert(right_chunk);
            right_chunk->left_edge_light_map[block_coords.y * Chunk::Depth + block_coords.z].sky_light_level = light_level;
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, right_chunk, sub_chunk_index);
        }

        if (block_coords.z == 0)
        {
            Chunk *front_chunk = chunk->neighbours[ChunkNeighbour_Front];
            Assert(front_chunk);
            front_chunk->back_edge_light_map[block_coords.y * Chunk::Width + block_coords.x].sky_light_level = light_level;
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, front_chunk, sub_chunk_index);
        }
        else if (block_coords.z == Chunk::Depth - 1)
        {
            Chunk *back_chunk = chunk->neighbours[ChunkNeighbour_Back];
            Assert(back_chunk);
            back_chunk->front_edge_light_map[block_coords.y * Chunk::Width + block_coords.x].sky_light_level = light_level;
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, back_chunk, sub_chunk_index);
        }

        i32 sub_chunk_start_y = sub_chunk_index * Chunk::SubChunkHeight;
        i32 sub_chunk_end_y = (sub_chunk_index + 1) * Chunk::SubChunkHeight - 1;

        if (block_coords.y == sub_chunk_end_y && sub_chunk_index != Chunk::SubChunkCount - 1)
        {
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, chunk, sub_chunk_index + 1);
        }
        else if (block_coords.y == sub_chunk_start_y && sub_chunk_index != 0)
        {
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, chunk, sub_chunk_index - 1);
        }
    }

    void set_block_light_source_level(World *world, Chunk *chunk, const glm::ivec3& block_coords, u8 light_level)
    {
        Block *block = get_block(chunk, block_coords);
        Block_Light_Info *light_info = get_block_light_info(chunk, block_coords);
        light_info->light_source_level = light_level;

        i32 sub_chunk_index = get_sub_chunk_render_data_index(block_coords);
        queue_update_sub_chunk_job(world->update_chunk_jobs_queue, chunk, sub_chunk_index);

        if (block_coords.x == 0)
        {
            Chunk *left_chunk = chunk->neighbours[ChunkNeighbour_Left];
            Assert(left_chunk);
            left_chunk->right_edge_light_map[block_coords.y * Chunk::Depth + block_coords.z].light_source_level = light_level;
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, left_chunk, sub_chunk_index);
        }
        else if (block_coords.x == Chunk::Width - 1)
        {
            Chunk *right_chunk = chunk->neighbours[ChunkNeighbour_Right];
            Assert(right_chunk);
            right_chunk->left_edge_light_map[block_coords.y * Chunk::Depth + block_coords.z].light_source_level = light_level;
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, right_chunk, sub_chunk_index);
        }

        if (block_coords.z == 0)
        {
            Chunk *front_chunk = chunk->neighbours[ChunkNeighbour_Front];
            Assert(front_chunk);
            front_chunk->back_edge_light_map[block_coords.y * Chunk::Width + block_coords.x].light_source_level = light_level;
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, front_chunk, sub_chunk_index);
        }
        else if (block_coords.z == Chunk::Depth - 1)
        {
            Chunk *back_chunk = chunk->neighbours[ChunkNeighbour_Back];
            Assert(back_chunk);
            back_chunk->front_edge_light_map[block_coords.y * Chunk::Width + block_coords.x].light_source_level = light_level;
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, back_chunk, sub_chunk_index);
        }

        i32 sub_chunk_start_y = sub_chunk_index * Chunk::SubChunkHeight;
        i32 sub_chunk_end_y = (sub_chunk_index + 1) * Chunk::SubChunkHeight - 1;

        if (block_coords.y == sub_chunk_end_y && sub_chunk_index != Chunk::SubChunkCount - 1)
        {
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, chunk, sub_chunk_index + 1);
        }
        else if (block_coords.y == sub_chunk_start_y && sub_chunk_index != 0)
        {
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, chunk, sub_chunk_index - 1);
        }
    }

    Block* get_block(World *world, const glm::vec3& position)
    {
       glm::ivec2 chunk_coords = world_position_to_chunk_coords(position);
       Chunk* chunk = get_chunk(world, chunk_coords);
       if (chunk)
       {
            glm::ivec3 block_coords = world_position_to_block_coords(world, position);
            return get_block(chunk, block_coords);
       }
       return &World::null_block;
    }

    Block_Query_Result query_block(World *world, const glm::vec3& position)
    {
        glm::ivec2 chunk_coords = world_position_to_chunk_coords(position);
        Chunk* chunk = get_chunk(world, chunk_coords);
        if (chunk)
        {
            glm::ivec3 block_coords = world_position_to_block_coords(world, position);
            if (block_coords.x >= 0 && block_coords.x < Chunk::Width  &&
                block_coords.y >= 0 && block_coords.y < Chunk::Height &&
                block_coords.z >= 0 && block_coords.z < Chunk::Width)
            {
                return { block_coords, get_block(chunk, block_coords), chunk };
            }
        }
        return { { -1, -1, -1 }, &World::null_block, nullptr };
    }

    Block_Query_Result query_neighbour_block_from_top(Chunk *chunk, const glm::ivec3& block_coords)
    {
        Block_Query_Result result;
        result.block_coords = { block_coords.x, block_coords.y + 1, block_coords.z };
        result.chunk = chunk;
        result.block = get_neighbour_block_from_top(chunk, block_coords);
        return result;
    }

    Block_Query_Result query_neighbour_block_from_bottom(Chunk *chunk, const glm::ivec3& block_coords)
    {
        Block_Query_Result result;
        result.block_coords = { block_coords.x, block_coords.y - 1, block_coords.z };
        result.chunk = chunk;
        result.block = get_neighbour_block_from_bottom(result.chunk, block_coords);
        return result;
    }

    Block_Query_Result query_neighbour_block_from_left(Chunk *chunk, const glm::ivec3& block_coords)
    {
        Block_Query_Result result;

        if (block_coords.x == 0)
        {
            result.chunk = chunk->neighbours[ChunkNeighbour_Left];
            result.block_coords = { Chunk::Width - 1, block_coords.y, block_coords.z };
        }
        else
        {
            result.chunk = chunk;
            result.block_coords = { block_coords.x - 1, block_coords.y, block_coords.z };
        }

        result.block = get_block(result.chunk, result.block_coords);
        return result;
    }

    Block_Query_Result query_neighbour_block_from_right(Chunk *chunk, const glm::ivec3& block_coords)
    {
        Block_Query_Result result;

        if (block_coords.x == Chunk::Width - 1)
        {
            result.chunk = chunk->neighbours[ChunkNeighbour_Right];
            result.block_coords = { 0, block_coords.y, block_coords.z };
        }
        else
        {
            result.chunk = chunk;
            result.block_coords = { block_coords.x + 1, block_coords.y, block_coords.z };
        }

        result.block = get_block(result.chunk, result.block_coords);
        return result;
    }

    Block_Query_Result query_neighbour_block_from_front(Chunk *chunk, const glm::ivec3& block_coords)
    {
        Block_Query_Result result;

        if (block_coords.z == 0)
        {
            result.chunk = chunk->neighbours[ChunkNeighbour_Front];
            result.block_coords = { block_coords.x, block_coords.y, Chunk::Depth - 1 };
        }
        else
        {
            result.chunk = chunk;
            result.block_coords = { block_coords.x, block_coords.y, block_coords.z - 1 };
        }

        result.block = get_block(result.chunk, result.block_coords);
        return result;
    }

    Block_Query_Result query_neighbour_block_from_back(Chunk *chunk, const glm::ivec3& block_coords)
    {
        Block_Query_Result result;
        if (block_coords.z == Chunk::Depth - 1)
        {
            result.chunk = chunk->neighbours[ChunkNeighbour_Back];
            result.block_coords = { block_coords.x, block_coords.y, 0 };
        }
        else
        {
            result.chunk = chunk;
            result.block_coords = { block_coords.x, block_coords.y, block_coords.z + 1 };
        }
        result.block = get_block(result.chunk, result.block_coords);
        return result;
    }

    void save_chunks(World *world)
    {
        for (u32 i = 0; i < World::ChunkHashTableCapacity; i++)
        {
            if (get_entry_state(world->chunk_hash_table_values[i]) != ChunkHashTableEntryState_Occupied)
            {
                continue;
            }

            u16 chunk_node_index = get_entry_value(world->chunk_hash_table_values[i]);
            Chunk *chunk = &world->chunk_nodes[chunk_node_index].chunk;
            Assert(chunk);

            if (chunk->state > ChunkState_Initialized)
            {
                chunk->state = ChunkState_PendingForSave;

                Serialize_Chunk_Job job;
                job.world = world;
                job.chunk = chunk;
                Job_System::schedule(job);
            }
        }

        Job_System::wait_for_jobs_to_finish();
    }

    std::array< Block_Query_Result, 6 > query_neighbours(Chunk *chunk, const glm::ivec3& block_coords)
    {
        std::array< Block_Query_Result, 6 > neighbours;
        neighbours[BlockNeighbour_Up]    = query_neighbour_block_from_top(chunk,    block_coords);
        neighbours[BlockNeighbour_Down]  = query_neighbour_block_from_bottom(chunk, block_coords);
        neighbours[BlockNeighbour_Left]  = query_neighbour_block_from_left(chunk,   block_coords);
        neighbours[BlockNeighbour_Right] = query_neighbour_block_from_right(chunk,  block_coords);
        neighbours[BlockNeighbour_Front] = query_neighbour_block_from_front(chunk,  block_coords);
        neighbours[BlockNeighbour_Back]  = query_neighbour_block_from_back(chunk,   block_coords);
        return neighbours;
    }

    const Block_Info World::block_infos[BlockId_Count] =
    {
        // Air
        {
            "air",
            0,
            0,
            0,
            BlockFlags_IsTransparent
        },
        // Grass
        {
            "grass",
            Texture_Id_grass_block_top,
            Texture_Id_dirt,
            Texture_Id_grass_block_side,
            BlockFlags_IsSolid | BlockFlags_ColorTopByBiome
        },
        // Sand
        {
            "sand",
            Texture_Id_sand,
            Texture_Id_sand,
            Texture_Id_sand,
            BlockFlags_IsSolid
        },
        // Dirt
        {
            "dirt",
            Texture_Id_dirt,
            Texture_Id_dirt,
            Texture_Id_dirt,
            BlockFlags_IsSolid
        },
        // Stone
        {
            "stone",
            Texture_Id_stone,
            Texture_Id_stone,
            Texture_Id_stone,
            BlockFlags_IsSolid
        },
        // Green Concrete
        {
            "green_concrete",
            Texture_Id_green_concrete_powder,
            Texture_Id_green_concrete_powder,
            Texture_Id_green_concrete_powder,
            BlockFlags_IsSolid
        },
        // BedRock
        {
            "bedrock",
            Texture_Id_bedrock,
            Texture_Id_bedrock,
            Texture_Id_bedrock,
            BlockFlags_IsSolid,
        },
        // Oak Log
        {
            "oak_log",
            Texture_Id_oak_log_top,
            Texture_Id_oak_log_top,
            Texture_Id_oak_log,
            BlockFlags_IsSolid,
        },
        // Oak Leaves
        {
            "oak_leaves",
            Texture_Id_oak_leaves,
            Texture_Id_oak_leaves,
            Texture_Id_oak_leaves,
            BlockFlags_IsSolid |
            BlockFlags_IsTransparent |
            BlockFlags_ColorTopByBiome |
            BlockFlags_ColorSideByBiome |
            BlockFlags_ColorBottomByBiome
        },
        // Oak Planks
        {
            "oak_planks",
            Texture_Id_oak_planks,
            Texture_Id_oak_planks,
            Texture_Id_oak_planks,
            BlockFlags_IsSolid
        },
        // Glow Stone
        {
            "glow_stone",
            Texture_Id_glowstone,
            Texture_Id_glowstone,
            Texture_Id_glowstone,
            BlockFlags_IsSolid | BlockFlags_IsLightSource
        },
        // Cobble Stone
        {
            "cobble_stone",
            Texture_Id_cobblestone,
            Texture_Id_cobblestone,
            Texture_Id_cobblestone,
            BlockFlags_IsSolid
        },
        // Spruce Log
        {
            "spruce_log",
            Texture_Id_spruce_log_top,
            Texture_Id_spruce_log_top,
            Texture_Id_spruce_log,
            BlockFlags_IsSolid
        },
        // Spruce Planks
        {
            "spruce_planks",
            Texture_Id_spruce_planks,
            Texture_Id_spruce_planks,
            Texture_Id_spruce_planks,
            BlockFlags_IsSolid
        },
        // Glass
        {
            "glass",
            Texture_Id_glass,
            Texture_Id_glass,
            Texture_Id_glass,
            BlockFlags_IsSolid | BlockFlags_IsTransparent
        },
        // Sea Lantern
        {
            "sea lantern",
            Texture_Id_sea_lantern,
            Texture_Id_sea_lantern,
            Texture_Id_sea_lantern,
            BlockFlags_IsSolid
        },
        // Birch Log
        {
            "birch log",
            Texture_Id_birch_log_top,
            Texture_Id_birch_log_top,
            Texture_Id_birch_log,
            BlockFlags_IsSolid
        },
        // Blue Stained Glass
        {
            "blue stained glass",
            Texture_Id_blue_stained_glass,
            Texture_Id_blue_stained_glass,
            Texture_Id_blue_stained_glass,
            BlockFlags_IsSolid | BlockFlags_IsTransparent,
        },
        // Water
        {
            "water",
            Texture_Id_water,
            Texture_Id_water,
            Texture_Id_water,
            BlockFlags_IsTransparent,
        },
        // Birch Planks
        {
            "birch_planks",
            Texture_Id_birch_planks,
            Texture_Id_birch_planks,
            Texture_Id_birch_planks,
            BlockFlags_IsSolid
        },
        // Diamond
        {
            "diamond",
            Texture_Id_diamond_block,
            Texture_Id_diamond_block,
            Texture_Id_diamond_block,
            BlockFlags_IsSolid
        },
        // Obsidian
        {
            "obsidian",
            Texture_Id_obsidian,
            Texture_Id_obsidian,
            Texture_Id_obsidian,
            BlockFlags_IsSolid
        },
        // Crying Obsidian
        {
            "crying_obsidian",
            Texture_Id_crying_obsidian,
            Texture_Id_crying_obsidian,
            Texture_Id_crying_obsidian,
            BlockFlags_IsSolid
        },
        // Dark Oak Log
        {
            "dark_oak_log",
            Texture_Id_dark_oak_log_top,
            Texture_Id_dark_oak_log_top,
            Texture_Id_dark_oak_log,
            BlockFlags_IsSolid
        },
        // Dark Oak Planks
        {
            "dark_oak_planks",
            Texture_Id_dark_oak_planks,
            Texture_Id_dark_oak_planks,
            Texture_Id_dark_oak_planks,
            BlockFlags_IsSolid
        },
        // Jungle Log
        {
            "jungle_log",
            Texture_Id_jungle_log_top,
            Texture_Id_jungle_log_top,
            Texture_Id_jungle_log,
            BlockFlags_IsSolid
        },
        // Jungle Planks
        {
            "jungle_planks",
            Texture_Id_jungle_planks,
            Texture_Id_jungle_planks,
            Texture_Id_jungle_planks,
            BlockFlags_IsSolid
        },
        // Acacia Log
        {
            "acacia_log",
            Texture_Id_acacia_log_top,
            Texture_Id_acacia_log_top,
            Texture_Id_acacia_log,
            BlockFlags_IsSolid
        },
        // Acacia Planks
        {
            "acacia_planks",
            Texture_Id_acacia_planks,
            Texture_Id_acacia_planks,
            Texture_Id_acacia_planks,
            BlockFlags_IsSolid
        }
    };

    Block World::null_block = { BlockId_Air };
}