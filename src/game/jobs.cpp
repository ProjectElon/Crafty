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
        namespace fs = std::filesystem;

        Load_Chunk_Job* data = (Load_Chunk_Job*)job_data;

        World* world = data->world;
        Chunk* chunk = data->chunk;

        auto& loaded_chunks = world->loaded_chunks;

        generate_chunk(chunk, world->seed);

        // todo(harlequin): use File_System
        if (fs::exists(fs::path(chunk->file_path)))
        {
            deserialize_chunk(chunk);
        }

        chunk->loaded           = true;
        chunk->pending_for_load = false;
    }

    void Update_Chunk_Job::execute(void* job_data)
    {
        Update_Chunk_Job* data = (Update_Chunk_Job*)job_data;
        World *world = data->world;
        Chunk *chunk = data->chunk;

        for (i32 sub_chunk_index = World::sub_chunk_count_per_chunk - 1; sub_chunk_index >= 0; sub_chunk_index--)
        {
            Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];
            if (render_data.pending_for_update)
            {
                Opengl_Renderer::update_sub_chunk(world, chunk, sub_chunk_index);
                render_data.pending_for_update = false;
            }
        }
        chunk->pending_for_update = false;
    }

    void Serialize_Chunk_Job::execute(void* job_data)
    {
        Serialize_Chunk_Job* data = (Serialize_Chunk_Job*)job_data;
        World *world = data->world;
        Chunk *chunk = data->chunk;
        serialize_chunk(chunk, world->seed, world->path);
        chunk->pending_for_save = false;
    }

    void Serialize_And_Free_Chunk_Job::execute(void* job_data)
    {
        Serialize_And_Free_Chunk_Job* data = (Serialize_And_Free_Chunk_Job*)job_data;
        World *world = data->world;
        Chunk *chunk = data->chunk;
        for (i32 sub_chunk_index = 0; sub_chunk_index < World::sub_chunk_count_per_chunk; ++sub_chunk_index)
        {
            Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];

            if (!render_data.pending_for_update && render_data.uploaded_to_gpu)
            {
                Opengl_Renderer::free_sub_chunk(chunk, sub_chunk_index);
            }
        }
        serialize_chunk(chunk, world->seed, world->path);
        chunk->pending_for_save = false;
        chunk->unload = true;
    }
}