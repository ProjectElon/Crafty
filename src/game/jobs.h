#pragma once

#include "core/common.h"
#include <thread>
#include <glm/glm.hpp>

namespace minecraft {

    struct Chunk;
    struct World;
    struct World_Region_Bounds;
    struct Temprary_Memory_Arena;

    struct Load_Chunk_Job alignas(std::hardware_constructive_interference_size)
    {
        World *world;
        Chunk *chunk;
        static void execute(void* job_data, Temprary_Memory_Arena *temp_arena);
    };

    struct Calculate_Chunk_Light_Propagation_Job alignas(std::hardware_constructive_interference_size)
    {
        World *world;
        Chunk *chunk;
        static void execute(void* job_data, Temprary_Memory_Arena *temp_arena);
    };

    struct Calculate_Chunk_Lighting_Job alignas(std::hardware_constructive_interference_size)
    {
        World *world;
        Chunk *chunk;
        static void execute(void* job_data, Temprary_Memory_Arena *temp_arena);
    };

    struct Update_Chunk_Job alignas(std::hardware_constructive_interference_size)
    {
        World *world;
        Chunk *chunk;
        static void execute(void* job_data, Temprary_Memory_Arena *temp_arena);
    };

    struct Serialize_Chunk_Job alignas(std::hardware_constructive_interference_size)
    {
        World *world;
        Chunk *chunk;
        static void execute(void* job_data, Temprary_Memory_Arena *temp_arena);
    };

    struct Serialize_And_Free_Chunk_Job alignas(std::hardware_constructive_interference_size)
    {
        World *world;
        Chunk *chunk;
        static void execute(void* job_data, Temprary_Memory_Arena *temp_arena);
    };
}