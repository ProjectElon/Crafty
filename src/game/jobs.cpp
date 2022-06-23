#include "jobs.h"
#include "renderer/opengl_renderer.h"
#include "renderer/camera.h"
#include "game/world.h"
#include "game/job_system.h"
#include "game/game.h"

#include <filesystem>

namespace minecraft {

    void Load_Chunk_Job::execute(void* job_data)
    {
        auto& loaded_chunks = World::loaded_chunks;
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
        chunk->loaded = true;
    }

    void Update_Chunk_Job::execute(void* job_data)
    {
        Update_Chunk_Job* data = (Update_Chunk_Job*)job_data;
        Chunk *chunk = data->chunk;
        for (i32 sub_chunk_index = World::sub_chunk_count_per_chunk - 1; sub_chunk_index >= 0; sub_chunk_index--)
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
        for (i32 sub_chunk_index = 0; sub_chunk_index < World::sub_chunk_count_per_chunk; ++sub_chunk_index)
        {
            Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];

            if (!render_data.pending_for_update && render_data.uploaded_to_gpu)
            {
                Opengl_Renderer::free_sub_chunk(chunk, sub_chunk_index);
            }
        }
        chunk->serialize();
        chunk->pending_for_save = false;
        chunk->unload = true;
    }
}