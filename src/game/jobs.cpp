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

        for (i32 sub_chunk_index = 0; sub_chunk_index < World::sub_chunk_count_per_chunk; ++sub_chunk_index)
        {
            Opengl_Renderer::upload_sub_chunk_to_gpu(chunk, sub_chunk_index);
        }
    }

    void Calculate_Chunk_Lighting_Job::execute(void* job_data)
    {
        Calculate_Chunk_Lighting_Job* data = (Calculate_Chunk_Lighting_Job*)job_data;
        Chunk* chunk = data->chunk;
        
        chunk->calculate_lighting();
        chunk->pending_for_lighting = false;
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