#pragma once

#include "game/world.h"
#include "renderer/opengl_renderer.h"

#include <thread>
#include <filesystem>

namespace minecraft {

    struct Load_Chunk_Job alignas(std::hardware_constructive_interference_size)
    {
        Chunk* chunk;

        static void execute(void* job_data)
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

            for (i32 sub_chunk_index = 0; sub_chunk_index < World::sub_chunk_count_per_chunk; ++sub_chunk_index)
            {
                Opengl_Renderer::upload_sub_chunk_to_gpu(chunk, sub_chunk_index);
            }

            chunk->pending = false;
        }
    };

    struct Update_Sub_Chunk_Job alignas(std::hardware_constructive_interference_size)
    {
        Chunk* chunk;
        i32 sub_chunk_index;

        static void execute(void* job_data)
        {
            namespace fs = std::filesystem;

            Update_Sub_Chunk_Job* data = (Update_Sub_Chunk_Job*)job_data;
            Chunk* chunk = data->chunk;
            i32 sub_chunk_index = data->sub_chunk_index;
            Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];
            Opengl_Renderer::update_sub_chunk(chunk, sub_chunk_index);
        }
    };

    struct Serialize_Chunk_Job alignas(std::hardware_constructive_interference_size)
    {
        Chunk* chunk;

        static void execute(void* job_data)
        {
            Serialize_Chunk_Job* data = (Serialize_Chunk_Job*)job_data;
            Chunk* chunk = data->chunk;
            chunk->serialize();
            chunk->pending = false;
        }
    };

    struct Serialize_And_Free_Chunk_Job alignas(std::hardware_constructive_interference_size)
    {
        Chunk* chunk;

        static void execute(void* job_data)
        {
            Serialize_And_Free_Chunk_Job* data = (Serialize_And_Free_Chunk_Job*)job_data;
            Chunk* chunk = data->chunk;
            chunk->serialize();
            chunk->pending = false;

            World::chunk_pool_mutex.lock();
            World::chunk_pool.reclame(chunk);
            World::chunk_pool_mutex.unlock();
        }
    };
}