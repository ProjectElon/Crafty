#include "jobs.h"
#include "renderer/opengl_renderer.h"
#include "renderer/camera.h"
#include "core/file_system.h"
#include "memory/memory_arena.h"
#include "game/world.h"
#include "game/job_system.h"
#include "game/game.h"
#include "ui/dropdown_console.h"

namespace minecraft {

    void Load_Chunk_Job::execute(void* job_data, Temprary_Memory_Arena *temp_arena)
    {
        Load_Chunk_Job* data = (Load_Chunk_Job*)job_data;

        World* world = data->world;
        Chunk* chunk = data->chunk;

        String8 chunk_file_path = get_chunk_file_path(world, chunk, temp_arena);

        generate_chunk(chunk, world->seed);

        if (File_System::exists(chunk_file_path.data))
        {
            deserialize_chunk(world, chunk, temp_arena);
        }

        chunk->state = ChunkState_Loaded;
    }

    void Update_Chunk_Job::execute(void* job_data, Temprary_Memory_Arena *temp_arena)
    {
        Update_Chunk_Job* data = (Update_Chunk_Job*)job_data;
        World *world = data->world;
        Chunk *chunk = data->chunk;

        for (i32 sub_chunk_index = World::sub_chunk_count_per_chunk - 1; sub_chunk_index >= 0; sub_chunk_index--)
        {
            Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];
            if (render_data.pending_for_update)
            {
                opengl_renderer_update_sub_chunk(world, chunk, sub_chunk_index);
                render_data.pending_for_update = false;
            }
        }

        chunk->pending_for_tessellation = false;
        chunk->tessellated              = true;
    }

    void Serialize_Chunk_Job::execute(void* job_data, Temprary_Memory_Arena *temp_arena)
    {
        Serialize_Chunk_Job* data = (Serialize_Chunk_Job*)job_data;
        World *world = data->world;
        Chunk *chunk = data->chunk;
        serialize_chunk(world, chunk, world->seed, temp_arena);
        chunk->state = ChunkState_Saved;
    }

    void Serialize_And_Free_Chunk_Job::execute(void* job_data, Temprary_Memory_Arena *temp_arena)
    {
        Serialize_And_Free_Chunk_Job* data = (Serialize_And_Free_Chunk_Job*)job_data;
        World *world = data->world;
        Chunk *chunk = data->chunk;
        for (i32 sub_chunk_index = 0; sub_chunk_index < World::sub_chunk_count_per_chunk; ++sub_chunk_index)
        {
            Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];

            if (!render_data.pending_for_update && render_data.uploaded_to_gpu)
            {
                opengl_renderer_free_sub_chunk(chunk, sub_chunk_index);
            }
        }
        serialize_chunk(world, chunk, world->seed, temp_arena);
        chunk->state = ChunkState_Saved;
    }
}