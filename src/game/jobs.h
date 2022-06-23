#pragma once

#include "core/common.h"
#include <thread>
#include <glm/glm.hpp>

namespace minecraft {

    struct Chunk;
    struct World_Region_Bounds;

    struct Load_Chunk_Job alignas(std::hardware_constructive_interference_size)
    {
        Chunk* chunk;
        static void execute(void* job_data);
    };

    struct Calculate_Chunk_Lighting_Job alignas(std::hardware_constructive_interference_size)
    {
        Chunk* chunk;
        static void execute(void* job_data);
    };

    struct Update_Chunk_Job alignas(std::hardware_constructive_interference_size)
    {
        Chunk* chunk;
        static void execute(void* job_data);
    };

    struct Serialize_Chunk_Job alignas(std::hardware_constructive_interference_size)
    {
        Chunk* chunk;
        static void execute(void* job_data);
    };

    struct Serialize_And_Free_Chunk_Job alignas(std::hardware_constructive_interference_size)
    {
        Chunk* chunk;
        static void execute(void* job_data);
    };
}