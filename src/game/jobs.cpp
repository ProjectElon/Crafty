#include "jobs.h"
#include "renderer/opengl_renderer.h"
#include "renderer/camera.h"
#include "game/world.h"
#include "game/game.h"

#include <filesystem>

namespace minecraft {

    void Load_Chunk_Job::execute(void* job_data)
    {
        namespace fs = std::filesystem;

        Load_Chunk_Job* data = (Load_Chunk_Job*)job_data;
        Chunk* chunk = data->chunk;

        if (!fs::exists(fs::path(chunk->file_path)))
        {
            chunk->generate(World::seed);
        }
        else
        {
            chunk->deserialize();
        }

        chunk->pending_for_load = false;
    }

    void Calculate_Chunks_Lighting_Job::execute(void* job_data)
    {
        World::light_mutex.lock();

        auto& loaded_chunks = World::loaded_chunks;

        Calculate_Chunks_Lighting_Job* data = (Calculate_Chunks_Lighting_Job*)job_data;
        World_Region_Bounds* region = data->region;

        for (i32 z = region->min.y; z <= region->max.y; ++z)
        {
            for (i32 x = region->min.x; x <= region->max.x; ++x)
            {
                glm::ivec2 chunk_coords = { x, z };
                auto it = loaded_chunks.find(chunk_coords);
                assert(it != loaded_chunks.end());
                Chunk* chunk = it->second;

                if (chunk->loaded && chunk->pending_for_lighting)
                {
                    bool is_all_neighour_chunks_loaded = true;

                    i32 dirs[] = { -1, 0, 1 };

                    for (i32 i = 0; i < 3; i++)
                    {
                        for (i32 j = 0; j < 3; j++)
                        {
                            i32 dx = dirs[i];
                            i32 dy = dirs[j];
                            auto it2 = loaded_chunks.find(chunk_coords + glm::ivec2(dx, dy));
                            if (it2 == loaded_chunks.end() || it2->second->pending_for_load || it2->second->loaded == false)
                            {
                                is_all_neighour_chunks_loaded = false;
                                break;
                            }
                        }
                    }

                    if (is_all_neighour_chunks_loaded)
                    {
                        chunk->calculate_lighting();
                        chunk->pending_for_lighting = false;
                    }
                }
            }
        }

        auto& light_queue = World::light_queue;

        while (!light_queue.empty())
        {
            auto block_query = light_queue.front();
            light_queue.pop();

            Block* block = block_query.block;
            const auto& info = block->get_info();
            auto neighbours_query = World::get_neighbours(block_query.chunk, block_query.block_coords);

            for (i32 d = 0; d < 6; d++)
            {
                auto& neighbour_query = neighbours_query[d];
                if (World::is_block_query_valid(neighbour_query))
                {
                    Block* neighbour = neighbour_query.block;
                    const auto& neighbour_info = neighbour->get_info();
                    if (neighbour_info.is_transparent())
                    {
                        if ((i32)neighbour->sky_light_level <= (i32)block->sky_light_level - 2)
                        {
                            World::set_block_sky_light_level(neighbour_query.chunk, neighbour_query.block_coords, (i32)block->sky_light_level - 1);
                            light_queue.push(neighbour_query);
                        }

                        if ((i32)neighbour->light_source_level <= (i32)block->light_source_level - 2)
                        {
                            World::set_block_light_source_level(neighbour_query.chunk, neighbour_query.block_coords, (i32)block->light_source_level - 1);
                            light_queue.push(neighbour_query);
                        }
                    }
                }
            }
        }

        auto& update_chunk_jobs_queue = World::update_chunk_jobs_queue;

        while (!update_chunk_jobs_queue.empty())
        {
            World::update_chunk_mutex.lock();
            const auto& job = update_chunk_jobs_queue.front();
            update_chunk_jobs_queue.pop();
            World::update_chunk_mutex.unlock();

            Chunk *chunk = job.chunk;

            for (i32 sub_chunk_index = 0; sub_chunk_index < World::sub_chunk_count_per_chunk; ++sub_chunk_index)
            {
                Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];
                if (render_data.pending_for_update)
                {
                    Opengl_Renderer::update_sub_chunk(chunk, sub_chunk_index);
                    render_data.pending_for_update = false;
                }
            }

            chunk->pending_for_update = false;
        }

        World::light_mutex.unlock();
    }

    void Update_Chunks_Job::execute(void* job_data)
    {
        Update_Chunks_Job* data = (Update_Chunks_Job*)job_data;
        auto& update_chunk_jobs_queue = World::update_chunk_jobs_queue;

        while (!update_chunk_jobs_queue.empty())
        {
            World::update_chunk_mutex.lock();
            const auto& job = update_chunk_jobs_queue.front();
            update_chunk_jobs_queue.pop();
            World::update_chunk_mutex.unlock();

            Chunk *chunk = job.chunk;

            for (i32 sub_chunk_index = 0; sub_chunk_index < World::sub_chunk_count_per_chunk; ++sub_chunk_index)
            {
                Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];
                if (render_data.pending_for_update)
                {
                    Opengl_Renderer::update_sub_chunk(chunk, sub_chunk_index);
                    render_data.pending_for_update = false;
                }
            }

            chunk->pending_for_update = false;
        }
    }

    void Calculate_Chunk_Lighting_Job::execute(void* job_data)
    {
        Calculate_Chunk_Lighting_Job* data = (Calculate_Chunk_Lighting_Job*)job_data;
        Chunk *chunk = data->chunk;
        chunk->calculate_lighting();
        chunk->pending_for_lighting = false;
    }

    void Update_Chunk_Job::execute(void* job_data)
    {
        Update_Chunk_Job* data = (Update_Chunk_Job*)job_data;
        Chunk *chunk = data->chunk;

        for (i32 sub_chunk_index = 0; sub_chunk_index < World::sub_chunk_count_per_chunk; ++sub_chunk_index)
        {
            Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];
            if (render_data.pending_for_update)
            {
                Opengl_Renderer::update_sub_chunk(chunk, sub_chunk_index);
                render_data.pending_for_update = false;
            }
        }

        chunk->pending_for_update = false;
    }

    void Update_Sub_Chunk_Job::execute(void* job_data)
    {
        namespace fs = std::filesystem;

        Update_Sub_Chunk_Job* data = (Update_Sub_Chunk_Job*)job_data;
        Chunk* chunk = data->chunk;
        i32 sub_chunk_index = data->sub_chunk_index;
        Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];

        Opengl_Renderer::update_sub_chunk(chunk, sub_chunk_index);
        render_data.pending_for_update = false;
    }

    void Serialize_Chunk_Job::execute(void* job_data)
    {
        Serialize_Chunk_Job* data = (Serialize_Chunk_Job*)job_data;
        Chunk* chunk = data->chunk;
        chunk->serialize();
        chunk->pending_for_save = false;
    }

    void Serialize_And_Free_Chunk_Job::execute(void* job_data)
    {
        Serialize_And_Free_Chunk_Job* data = (Serialize_And_Free_Chunk_Job*)job_data;
        Chunk* chunk = data->chunk;
        chunk->serialize();
        chunk->pending_for_save = false;
        World::chunk_pool_mutex.lock();
        World::chunk_pool.reclame(chunk);
        World::chunk_pool_mutex.unlock();
    }
}