#include "world.h"
#include "renderer/opengl_renderer.h"
#include "game/job_system.h"
#include "ui/dropdown_console.h"
#include "game/jobs.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/noise.hpp>
#include <time.h>
#include <sstream>
#include <filesystem>
#include <errno.h>
#include <new>
#include <sstream>

#include "game/profiler.h"

extern int errno;

namespace minecraft {

    std::array< glm::ivec2, ChunkNeighbour_Count > get_chunk_neighbour_directions()
    {
        std::array< glm::ivec2, ChunkNeighbour_Count > directions;
        directions[ChunkNeighbour_Front]      = {  0, -1 };
        directions[ChunkNeighbour_Back]       = {  0,  1 };
        directions[ChunkNeighbour_Left]       = { -1,  0 };
        directions[ChunkNeighbour_Right]      = {  1,  0 };
        directions[ChunkNeighbour_FrontRight] = {  1, -1 };
        directions[ChunkNeighbour_FrontLeft]  = { -1, -1 };
        directions[ChunkNeighbour_BackRight]  = {  1,  1 };
        directions[ChunkNeighbour_BackLeft]   = { -1,  1 };
        return directions;
    }

    i32 get_block_index(const glm::ivec3& block_coords)
    {
        return block_coords.y * MC_CHUNK_WIDTH * MC_CHUNK_DEPTH + block_coords.z * MC_CHUNK_WIDTH + block_coords.x;
    }

    glm::vec3 get_block_position(Chunk *chunk, const glm::ivec3& block_coords)
    {
        return chunk->position + glm::vec3((f32)block_coords.x + 0.5f, (f32)block_coords.y + 0.5f, (f32)block_coords.z + 0.5f);
    }

    Block* get_block(Chunk *chunk, const glm::ivec3& block_coords)
    {
        assert(block_coords.x >= 0 && block_coords.x < MC_CHUNK_WIDTH &&
               block_coords.y >= 0 && block_coords.y < MC_CHUNK_HEIGHT &&
               block_coords.z >= 0 && block_coords.z < MC_CHUNK_DEPTH);

        i32 block_index = get_block_index(block_coords);
        return chunk->blocks + block_index;
    }

    Block_Light_Info* get_block_light_info(Chunk *chunk, const glm::ivec3& block_coords)
    {
        assert(block_coords.x >= 0 && block_coords.x < MC_CHUNK_WIDTH &&
               block_coords.y >= 0 && block_coords.y < MC_CHUNK_HEIGHT &&
               block_coords.z >= 0 && block_coords.z < MC_CHUNK_DEPTH);
        i32 block_index = get_block_index(block_coords);
        return chunk->light_map + block_index;
    }

    glm::ivec2 world_position_to_chunk_coords(const glm::vec3& position)
    {
        const f32 one_over_16 = 1.0f / 16.0f;
        return { glm::floor(position.x * one_over_16), glm::floor(position.z * one_over_16) };
    }

    bool initialize_chunk(Chunk *chunk, const glm::ivec2& world_coords, String8 world_path)
    {
        chunk->world_coords = world_coords;
        chunk->position = { world_coords.x * MC_CHUNK_WIDTH, 0.0f, world_coords.y * MC_CHUNK_DEPTH };

        for (i32 sub_chunk_index = 0; sub_chunk_index < World::sub_chunk_count_per_chunk; ++sub_chunk_index)
        {
            Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];

            render_data.face_count      = 0;
            render_data.bucket_index    = 0;
            render_data.instance_memory_id = -1;

            for (i32 j = 0; j < 2; j++)
            {
                render_data.opaque_buckets[j].memory_id = -1;
                render_data.opaque_buckets[j].current_vertex = nullptr;
                render_data.opaque_buckets[j].face_count = 0;

                render_data.transparent_buckets[j].memory_id = -1;
                render_data.transparent_buckets[j].current_vertex = nullptr;
                render_data.transparent_buckets[j].face_count = 0;

                constexpr f32 infinity = std::numeric_limits<f32>::max();
                render_data.aabb[j] = { { infinity, infinity, infinity }, { -infinity, -infinity, -infinity } };
            }

            render_data.uploaded_to_gpu = false;
            render_data.pending_for_update = false;
        }

        chunk->loaded               = false;
        chunk->pending_for_load     = true;
        chunk->neighbours_loaded    = false;
        chunk->pending_for_lighting = true;
        chunk->in_light_propagation_queue = false;
        chunk->in_light_calculation_queue = false;
        chunk->light_propagated     = false;
        chunk->light_calculated     = false;
        chunk->pending_for_save     = false;
        chunk->pending_for_update   = false;
        chunk->unload               = false;

        for (i32 i = 0; i < ChunkNeighbour_Count; i++)
        {
            chunk->neighbours[i] = nullptr;
        }

        sprintf(chunk->file_path,
                "%s/chunk_%d_%d.pkg",
                world_path.data,
                chunk->world_coords.x,
                chunk->world_coords.y);

        return true;
    }

    inline static glm::vec2 get_sample(i32 seed, const glm::ivec2& chunk_coords, const glm::ivec2& block_xz_coords)
    {
        return {
            seed + chunk_coords.x * MC_CHUNK_WIDTH + block_xz_coords.x + 0.5f,
            seed + chunk_coords.y * MC_CHUNK_DEPTH + block_xz_coords.y + 0.5f };
    }

    inline static f32 get_noise01(const glm::vec2& sample)
    {
        const i32 octaves = 5;
        f32 scales[octaves]  = { 0.002f, 0.005f, 0.04f , 0.015f, 0.004f };
        f32 weights[octaves] = { 0.6f, 0.2f, 0.05f, 0.1f, 0.05f };
        f32 noise = 0.0f;

        for (i32 i = 0; i < octaves; i++)
        {
            f32 value = glm::simplex(sample * scales[i]);
            value = (value + 1.0f) / 2.0f;
            noise += value * weights[i];
        }

        return noise;
    }

    inline static i32 get_height_from_noise01(i32 min_height, i32 max_height, f32 noise)
    {
        return (i32)glm::trunc(min_height + ((max_height - min_height) * noise));
    }

    static void set_block_id_based_on_height(Block *block,
                                             i32 block_y,
                                             i32 height,
                                             i32 min_height,
                                             i32 max_height,
                                             i32 water_level)
    {
        if (block_y > height)
        {
            if (block_y < water_level)
            {
                block->id = BlockId_Water;
            }
            else
            {
                block->id = BlockId_Air;
            }
        }
        else if (block_y == height)
        {
            block->id = BlockId_Grass;
        }
        else
        {
            block->id = BlockId_Dirt;
        }
    }

    void generate_chunk(Chunk *chunk, i32 seed)
    {
        i32 height_map[MC_CHUNK_DEPTH][MC_CHUNK_WIDTH];

        i32 top_edge_height_map[MC_CHUNK_WIDTH];
        i32 bottom_edge_height_map[MC_CHUNK_WIDTH];
        i32 left_edge_height_map[MC_CHUNK_DEPTH];
        i32 right_edge_height_map[MC_CHUNK_DEPTH];

        constexpr i32 min_biome_height = 100;
        constexpr i32 max_biome_height = 250;
        constexpr i32 water_level = min_biome_height + 50;
        static_assert(water_level >= min_biome_height && water_level <= max_biome_height);

        const glm::ivec2 front_chunk_coords = { chunk->world_coords.x + 0, chunk->world_coords.y - 1 };
        const glm::ivec2 back_chunk_coords  = { chunk->world_coords.x + 0, chunk->world_coords.y + 1 };
        const glm::ivec2 left_chunk_coords  = { chunk->world_coords.x - 1, chunk->world_coords.y + 0 };
        const glm::ivec2 right_chunk_coords = { chunk->world_coords.x + 1, chunk->world_coords.y + 0 };

        for (i32 z = 0; z < MC_CHUNK_DEPTH; ++z)
        {
            for (i32 x = 0; x < MC_CHUNK_WIDTH; ++x)
            {
                glm::ivec2 block_xz_coords = { x, z };
                glm::vec2 sample = get_sample(seed, chunk->world_coords, block_xz_coords);
                f32 noise = get_noise01(sample);
                assert(noise >= 0.0f && noise <= 1.0f);
                height_map[z][x] = get_height_from_noise01(min_biome_height, max_biome_height, noise);
            }
        }

        for (i32 x = 0; x < MC_CHUNK_WIDTH; ++x)
        {
            glm::ivec2 block_xz_coords = { x, MC_CHUNK_DEPTH - 1 };
            glm::vec2 sample = get_sample(seed, front_chunk_coords, block_xz_coords);
            f32 noise = get_noise01(sample);
            top_edge_height_map[x] = get_height_from_noise01(min_biome_height, max_biome_height, noise);
        }

        for (i32 x = 0; x < MC_CHUNK_WIDTH; ++x)
        {
            glm::ivec2 block_xz_coords = { x, 0 };
            glm::vec2 sample = get_sample(seed, back_chunk_coords, block_xz_coords);
            f32 noise = get_noise01(sample);
            bottom_edge_height_map[x] = get_height_from_noise01(min_biome_height, max_biome_height, noise);
        }

        for (i32 z = 0; z < MC_CHUNK_DEPTH; ++z)
        {
            glm::ivec2 block_xz_coords = { MC_CHUNK_WIDTH - 1, z };
            glm::vec2 sample = get_sample(seed, left_chunk_coords, block_xz_coords);
            f32 noise = get_noise01(sample);
            left_edge_height_map[z] = get_height_from_noise01(min_biome_height, max_biome_height, noise);
        }

        for (i32 z = 0; z < MC_CHUNK_DEPTH; ++z)
        {
            glm::ivec2 block_xz_coords = { 0, z };
            glm::vec2 sample = get_sample(seed, right_chunk_coords, block_xz_coords);
            f32 noise = get_noise01(sample);
            right_edge_height_map[z] = get_height_from_noise01(min_biome_height, max_biome_height, noise);
        }

        for (i32 y = 0; y < MC_CHUNK_HEIGHT; ++y)
        {
            for (i32 z = 0; z < MC_CHUNK_DEPTH; ++z)
            {
                for (i32 x = 0; x < MC_CHUNK_WIDTH; ++x)
                {
                    glm::ivec3 block_coords = { x, y, z };
                    Block *block = get_block(chunk, block_coords);
                    const i32& height = height_map[z][x];
                    set_block_id_based_on_height(block, y, height, min_biome_height, max_biome_height, water_level);
                }
            }
        }

        for (i32 y = 0; y < MC_CHUNK_HEIGHT; ++y)
        {
            for (i32 x = 0; x < MC_CHUNK_WIDTH; ++x)
            {
                const i32& top_edge_height = top_edge_height_map[x];
                Block *top_edge_block = &(chunk->front_edge_blocks[y * MC_CHUNK_WIDTH + x]);
                set_block_id_based_on_height(top_edge_block, y, top_edge_height, min_biome_height, max_biome_height, water_level);

                const i32& bottom_edge_height = bottom_edge_height_map[x];
                Block *bottom_edge_block = &(chunk->back_edge_blocks[y * MC_CHUNK_WIDTH + x]);
                set_block_id_based_on_height(bottom_edge_block, y, bottom_edge_height, min_biome_height, max_biome_height, water_level);
            }

            for (i32 z = 0; z < MC_CHUNK_DEPTH; ++z)
            {
                const i32& left_edge_height = left_edge_height_map[z];
                Block *left_edge_block = &(chunk->left_edge_blocks[y * MC_CHUNK_DEPTH + z]);
                set_block_id_based_on_height(left_edge_block, y, left_edge_height, min_biome_height, max_biome_height, water_level);

                const i32& right_edge_height = right_edge_height_map[z];
                Block *right_edge_block = &(chunk->right_edge_blocks[y * MC_CHUNK_DEPTH + z]);
                set_block_id_based_on_height(right_edge_block, y, right_edge_height, min_biome_height, max_biome_height, water_level);
            }
        }
    }

    struct Chunk_Serialization_Header
    {
        u32 block_count;
        u32 front_edge_block_count;
        u32 back_edge_block_count;
        u32 left_edge_block_count;
        u32 right_edge_block_count;
    };

    struct Block_Serialization_Info
    {
        u16 block_index;
        u16 block_id;
    };

    void serialize_chunk(Chunk *chunk, i32 seed, String8 world_path)
    {
        assert(chunk->loaded);

        // todo(harlequin): temprary
        Chunk *original_chunk = new Chunk;
        initialize_chunk(original_chunk, chunk->world_coords, world_path);
         generate_chunk(original_chunk, seed);

        u32 block_count = 0;
        Block_Serialization_Info serialized_blocks[MC_CHUNK_HEIGHT * MC_CHUNK_DEPTH * MC_CHUNK_WIDTH];

        for (i32 block_index = 0; block_index < MC_CHUNK_HEIGHT * MC_CHUNK_DEPTH * MC_CHUNK_WIDTH; block_index++)
        {
            Block& block = chunk->blocks[block_index];
            Block& original_block = original_chunk->blocks[block_index];

            if (block.id != original_block.id)
            {
                Block_Serialization_Info& info = serialized_blocks[block_count++];
                info.block_index = (u16)block_index;
                info.block_id    = block.id;
            }
        }

        u32 front_edge_count = 0;
        Block_Serialization_Info serialized_front_edge_blocks[MC_CHUNK_HEIGHT * MC_CHUNK_WIDTH];

        for (u32 block_index = 0; block_index < MC_CHUNK_HEIGHT * MC_CHUNK_WIDTH; block_index++)
        {
            Block& block = chunk->front_edge_blocks[block_index];
            Block& original_block = original_chunk->front_edge_blocks[block_index];

            if (block.id != original_block.id)
            {
                Block_Serialization_Info& info = serialized_front_edge_blocks[front_edge_count++];
                info.block_index = (u16)block_index;
                info.block_id    = block.id;
            }
        }

        u32 back_edge_count = 0;
        Block_Serialization_Info serialized_back_edge_blocks[MC_CHUNK_HEIGHT  * MC_CHUNK_WIDTH];

        for (u32 block_index = 0; block_index < MC_CHUNK_HEIGHT * MC_CHUNK_WIDTH; block_index++)
        {
            Block& block = chunk->back_edge_blocks[block_index];
            Block& original_block = original_chunk->back_edge_blocks[block_index];

            if (block.id != original_block.id)
            {
                Block_Serialization_Info& info = serialized_back_edge_blocks[back_edge_count++];
                info.block_index = (u16)block_index;
                info.block_id    = block.id;
            }
        }

        u32 left_edge_count = 0;
        Block_Serialization_Info serialized_left_edge_blocks[MC_CHUNK_HEIGHT * MC_CHUNK_DEPTH];

        for (u32 block_index = 0; block_index < MC_CHUNK_HEIGHT * MC_CHUNK_WIDTH; block_index++)
        {
            Block& block = chunk->left_edge_blocks[block_index];
            Block& original_block = original_chunk->left_edge_blocks[block_index];

            if (block.id != original_block.id)
            {
                Block_Serialization_Info& info = serialized_left_edge_blocks[left_edge_count++];
                info.block_index = (u16)block_index;
                info.block_id    = block.id;
            }
        }

        u32 right_edge_count = 0;
        Block_Serialization_Info serialized_right_edge_blocks[MC_CHUNK_HEIGHT * MC_CHUNK_DEPTH];

        for (u32 block_index = 0; block_index < MC_CHUNK_HEIGHT * MC_CHUNK_WIDTH; block_index++)
        {
            Block& block = chunk->right_edge_blocks[block_index];
            Block& original_block = original_chunk->right_edge_blocks[block_index];

            if (block.id != original_block.id)
            {
                Block_Serialization_Info& info = serialized_right_edge_blocks[right_edge_count++];
                info.block_index = (u16)block_index;
                info.block_id    = block.id;
            }
        }

        u32 serialized_block_count = block_count + front_edge_count + back_edge_count + left_edge_count + right_edge_count;

        if (serialized_block_count)
        {

            FILE *file = fopen(chunk->file_path, "wb");

            if (file == NULL)
            {
                fprintf(stderr, "[ERROR]: failed to open file %s for writing: %s\n", chunk->file_path, strerror(errno));
                return;
            }

            Chunk_Serialization_Header header;
            header.block_count            = block_count;
            header.front_edge_block_count = front_edge_count;
            header.back_edge_block_count  = back_edge_count;
            header.left_edge_block_count  = left_edge_count;
            header.right_edge_block_count = right_edge_count;

            fwrite(&header, sizeof(Chunk_Serialization_Header), 1, file);

            if (block_count)
            {
                fwrite(serialized_blocks, sizeof(Block_Serialization_Info) * block_count, 1, file);
            }

            if (front_edge_count)
            {
                fwrite(serialized_front_edge_blocks, sizeof(Block_Serialization_Info) * front_edge_count, 1, file);
            }

            if (back_edge_count)
            {
                fwrite(serialized_back_edge_blocks,  sizeof(Block_Serialization_Info) * back_edge_count,  1, file);
            }

            if (left_edge_count)
            {
                fwrite(serialized_left_edge_blocks,  sizeof(Block_Serialization_Info) * left_edge_count,  1, file);
            }

            if (right_edge_count)
            {
                fwrite(serialized_right_edge_blocks, sizeof(Block_Serialization_Info) * right_edge_count, 1, file);
            }

            fclose(file);
        }
        else
        {
            // todo(harlequin): use File_System
            if (std::filesystem::exists(std::filesystem::path(chunk->file_path)))
            {
                i32 result = remove(chunk->file_path);

                if (result != 0)
                {
                    fprintf(stderr, "[ERROR]: failed to delete file: %s\n", chunk->file_path);
                }
            }
        }

        delete original_chunk;
    }

    void deserialize_chunk(Chunk *chunk)
    {
        assert(!chunk->loaded);

        FILE *file = fopen(chunk->file_path, "rb");
        if (file == NULL)
        {
            fprintf(stderr, "[ERROR]: failed to open file %s for reading: %s\n", chunk->file_path, strerror(errno));
            return;
        }

        Chunk_Serialization_Header header;
        fread(&header, sizeof(Chunk_Serialization_Header), 1, file);

        if (header.block_count)
        {
            Block_Serialization_Info serialized_blocks[MC_CHUNK_HEIGHT * MC_CHUNK_DEPTH * MC_CHUNK_WIDTH];
            fread(serialized_blocks, sizeof(Block_Serialization_Info) * header.block_count, 1, file);

            for (u32 i = 0; i < header.block_count; i++)
            {
                Block_Serialization_Info& info = serialized_blocks[i];
                Block* block = chunk->blocks + info.block_index;
                block->id = info.block_id;
            }
        }

        if (header.front_edge_block_count)
        {
            Block_Serialization_Info serialized_front_edge_blocks[MC_CHUNK_HEIGHT * MC_CHUNK_WIDTH];
            fread(serialized_front_edge_blocks, sizeof(Block_Serialization_Info) * header.front_edge_block_count, 1, file);

            for (u32 i = 0; i < header.front_edge_block_count; i++)
            {
                Block_Serialization_Info& info = serialized_front_edge_blocks[i];
                Block* block = chunk->front_edge_blocks + info.block_index;
                block->id = info.block_id;
            }
        }

        if (header.back_edge_block_count)
        {
            Block_Serialization_Info serialized_back_edge_blocks[MC_CHUNK_HEIGHT * MC_CHUNK_WIDTH];
            fread(serialized_back_edge_blocks, sizeof(Block_Serialization_Info) * header.back_edge_block_count, 1, file);

            for (u32 i = 0; i < header.back_edge_block_count; i++)
            {
                Block_Serialization_Info& info = serialized_back_edge_blocks[i];
                Block* block = chunk->back_edge_blocks + info.block_index;
                block->id = info.block_id;
            }
        }

        if (header.left_edge_block_count)
        {
            Block_Serialization_Info serialized_left_edge_blocks[MC_CHUNK_HEIGHT * MC_CHUNK_DEPTH];
            fread(serialized_left_edge_blocks, sizeof(Block_Serialization_Info) * header.left_edge_block_count, 1, file);

            for (u32 i = 0; i < header.left_edge_block_count; i++)
            {
                Block_Serialization_Info& info = serialized_left_edge_blocks[i];
                Block* block = chunk->left_edge_blocks + info.block_index;
                block->id = info.block_id;
            }
        }

        if (header.right_edge_block_count)
        {
            Block_Serialization_Info serialized_right_edge_blocks[MC_CHUNK_HEIGHT * MC_CHUNK_DEPTH];
            fread(serialized_right_edge_blocks, sizeof(Block_Serialization_Info) * header.right_edge_block_count, 1, file);

            for (u32 i = 0; i < header.right_edge_block_count; i++)
            {
                Block_Serialization_Info& info = serialized_right_edge_blocks[i];
                Block* block = chunk->right_edge_blocks + info.block_index;
                block->id = info.block_id;
            }
        }

        fclose(file);
    }

    Block* get_neighbour_block_from_right(Chunk *chunk, const glm::ivec3& block_coords)
    {
        if (block_coords.x == MC_CHUNK_WIDTH - 1)
        {
            return &(chunk->right_edge_blocks[block_coords.y * MC_CHUNK_DEPTH + block_coords.z]);
        }

        return get_block(chunk, { block_coords.x + 1, block_coords.y, block_coords.z });
    }

    Block* get_neighbour_block_from_left(Chunk *chunk, const glm::ivec3& block_coords)
    {
        if (block_coords.x == 0)
        {
            return &(chunk->left_edge_blocks[block_coords.y * MC_CHUNK_DEPTH + block_coords.z]);
        }

        return get_block(chunk, { block_coords.x - 1, block_coords.y, block_coords.z });
    }

    Block* get_neighbour_block_from_top(Chunk *chunk, const glm::ivec3& block_coords)
    {
        if (block_coords.y == MC_CHUNK_HEIGHT - 1)
        {
            return &World::null_block;
        }
        return get_block(chunk, { block_coords.x, block_coords.y + 1, block_coords.z });
    }

    Block* get_neighbour_block_from_bottom(Chunk *chunk, const glm::ivec3& block_coords)
    {
        if (block_coords.y == 0)
        {
            return &World::null_block;
        }
        return get_block(chunk, { block_coords.x, block_coords.y - 1, block_coords.z });
    }

    Block* get_neighbour_block_from_front(Chunk *chunk, const glm::ivec3& block_coords)
    {
        if (block_coords.z == 0)
        {
            return &(chunk->front_edge_blocks[block_coords.y * MC_CHUNK_WIDTH + block_coords.x]);
        }
        return get_block(chunk, { block_coords.x, block_coords.y, block_coords.z - 1 });
    }

    Block* get_neighbour_block_from_back(Chunk *chunk, const glm::ivec3& block_coords)
    {
        if (block_coords.z == MC_CHUNK_DEPTH - 1)
        {
            return &(chunk->back_edge_blocks[block_coords.y * MC_CHUNK_WIDTH + block_coords.x]);
        }

        return get_block(chunk, { block_coords.x, block_coords.y, block_coords.z + 1 });
    }

    std::array<Block*, 6> get_neighbours(Chunk *chunk, const glm::ivec3& block_coords)
    {
        std::array<Block*, 6> neighbours;

        neighbours[BlockNeighbour_Up]    = get_neighbour_block_from_top(chunk,    block_coords);
        neighbours[BlockNeighbour_Down]  = get_neighbour_block_from_bottom(chunk, block_coords);
        neighbours[BlockNeighbour_Left]  = get_neighbour_block_from_left(chunk,   block_coords);
        neighbours[BlockNeighbour_Right] = get_neighbour_block_from_right(chunk,  block_coords);
        neighbours[BlockNeighbour_Front] = get_neighbour_block_from_front(chunk,  block_coords);
        neighbours[BlockNeighbour_Back]  = get_neighbour_block_from_back(chunk,   block_coords);

        return neighbours;
    }

    void propagate_sky_light(World *world, Chunk *chunk, Circular_FIFO_Queue<Block_Query_Result> *queue)
    {
        for (i32 z = 0; z < MC_CHUNK_DEPTH; z++)
        {
            for (i32 x = 0; x < MC_CHUNK_WIDTH; x++)
            {
                bool can_sky_light_propagate = true;

                for (i32 y = MC_CHUNK_HEIGHT - 1; y >= 0; y--)
                {
                    glm::ivec3 block_coords = { x, y, z };
                    Block *block = get_block(chunk, block_coords);
                    const Block_Info* info = get_block_info(world, block);

                    if (is_light_source(info))
                    {
                        set_block_light_source_level(world, chunk, block_coords, 15);

                        Block_Query_Result query = {};
                        query.block              = block;
                        query.block_coords       = block_coords;
                        query.chunk              = chunk;

                        queue->push(query);
                    }
                    else
                    {
                        set_block_light_source_level(world, chunk, block_coords, 1);
                    }

                    if (!is_block_transparent(info))
                    {
                        can_sky_light_propagate = false;
                    }

                    set_block_sky_light_level(world, chunk, block_coords, can_sky_light_propagate ? 15 : 1);
                }
            }
        }
    }

    void calculate_lighting(World *world, Chunk *chunk, Circular_FIFO_Queue<Block_Query_Result> *queue)
    {
        for (i32 y = MC_CHUNK_HEIGHT - 1; y >= 0; y--)
        {
            // bool found_any_sky_lights = false;
            for (i32 z = 0; z < MC_CHUNK_WIDTH; z++)
            {
                for (i32 x = 0; x < MC_CHUNK_WIDTH; x++)
                {
                    glm::ivec3 block_coords = { x, y, z };
                    Block* block = get_block(chunk, block_coords);
                    const Block_Info* info = get_block_info(world, block);
                    if (!is_block_transparent(info)) continue;
                    Block_Light_Info *block_light_info = get_block_light_info(chunk, block_coords);
                    if (block_light_info->sky_light_level == 15)
                    {
                        // found_any_sky_lights = true;
                        auto neighbours_query = world_get_neighbours(chunk, block_coords);
                        for (i32 direction = 2; direction < 6; direction++)
                        {
                            auto& neighbour_query = neighbours_query[direction];
                            Block *neighbour = neighbour_query.block;
                            const Block_Info* neighbour_info       = get_block_info(world, neighbour);
                            Block_Light_Info *neighbour_light_info = get_block_light_info(neighbour_query.chunk, neighbour_query.block_coords);
                            if (neighbour_light_info->sky_light_level != 15 && is_block_transparent(neighbour_info))
                            {
                                Block_Query_Result query = {};
                                query.block              = block;
                                query.block_coords       = block_coords;
                                query.chunk              = chunk;
                                queue->push(query);
                                break;
                            }
                        }
                    }
                }
            }
            // if (!found_any_sky_lights) break;
        }
    }

    // todo(harlequin): can't we pretend there is a chunk there instead of using get_chunk
    glm::ivec3 world_position_to_block_coords(World *world, const glm::vec3& position)
    {
        glm::ivec2 chunk_coords = world_position_to_chunk_coords(position);
        glm::vec3 offset        = position - get_chunk(world, chunk_coords)->position;
        return { (i32)glm::floor(offset.x), (i32)glm::floor(position.y), (i32)glm::floor(offset.z) };
    }

    World_Region_Bounds get_world_bounds_from_chunk_coords(i32 chunk_radius, const glm::ivec2& chunk_coords)
    {
        World_Region_Bounds bounds;
        bounds.min = chunk_coords - glm::ivec2(chunk_radius, chunk_radius);
        bounds.max = chunk_coords + glm::ivec2(chunk_radius, chunk_radius);
        return bounds;
    }

    bool is_block_query_valid(const Block_Query_Result& query)
    {
        return query.chunk && query.block &&
               query.block_coords.x >= 0 && query.block_coords.x < MC_CHUNK_WIDTH  &&
               query.block_coords.y >= 0 && query.block_coords.y < MC_CHUNK_HEIGHT &&
               query.block_coords.z >= 0 && query.block_coords.z < MC_CHUNK_DEPTH;
    }

    bool is_block_query_in_world_region(const Block_Query_Result& query, const World_Region_Bounds& bounds)
    {
        return query.chunk->world_coords.x >= bounds.min.x &&
               query.chunk->world_coords.x <= bounds.max.x &&
               query.chunk->world_coords.y >= bounds.min.y &&
               query.chunk->world_coords.y <= bounds.max.y;
    }

    i32 get_sub_chunk_index(const glm::ivec3& block_coords)
    {
        return block_coords.y / World::sub_chunk_height;
    }

    bool initialize_world(World *world, String8 world_path)
    {
        namespace fs = std::filesystem;

        // todo(harlequin): use File_System.h
        if (!fs::exists(fs::path(world_path.data)))
        {
            fs::create_directories(fs::path(world_path.data));
        }

        world->path = world_path;

        // @todo(harlequin): Temprary
        char meta_file_path[256];
        sprintf(meta_file_path, "%s/meta", world_path.data);

        FILE* meta_file = fopen(meta_file_path, "rb");

        if (meta_file)
        {
            fscanf(meta_file, "%d", &world->seed);
        }
        else
        {
            srand((u32)time(nullptr));
            world->seed = (i32)(((f32)rand() / (f32)RAND_MAX) * 1000000.0f);
            meta_file = fopen(meta_file_path, "wb");
            fprintf(meta_file, "%d", world->seed);
        }

        fclose(meta_file);

        world->loaded_chunks = robin_hood::unordered_node_map< glm::ivec2, Chunk*, Chunk_Hash >();
        world->loaded_chunks.reserve(2 * world->chunk_capacity);

        world->chunk_pool.initialize();

        world->pending_free_chunks = std::vector<Chunk*>();
        world->pending_free_chunks.reserve(world->chunk_capacity);

        world->update_chunk_jobs_queue.initialize();
        world->calculate_chunk_lighting_queue.initialize();
        world->light_propagation_queue.initialize();

        return true;
    }

    void shutdown_world(World *world)
    {
    }

    Block_Query_Result select_block(World *world,
                                    const glm::vec3& view_position,
                                    const glm::vec3& view_direction,
                                    u32              max_block_select_dist_in_cube_units,
                                    Ray_Cast_Result *out_ray_cast_result)
    {
        Block_Query_Result result;
        result.chunk = nullptr;
        result.block = nullptr;
        result.block_coords = { -1, -1, -1 };

        glm::vec3 query_position = view_position;

        for (u32 i = 0; i < max_block_select_dist_in_cube_units * 10; i++)
        {
            Block_Query_Result query = query_block(world, query_position);

            if (is_block_query_valid(query) && query.block->id != BlockId_Air && query.block->id != BlockId_Water)
            {
                glm::vec3 block_position = get_block_position(query.chunk, query.block_coords);
                Ray ray   = { view_position, view_direction };
                AABB aabb = { block_position - glm::vec3(0.5f, 0.5f, 0.5f), block_position + glm::vec3(0.5f, 0.5f, 0.5f) };
                Ray_Cast_Result ray_cast_result = cast_ray_on_aabb(ray, aabb, (f32)max_block_select_dist_in_cube_units);

                if (ray_cast_result.hit)
                {
                    result = query;
                    if (out_ray_cast_result) *out_ray_cast_result = ray_cast_result;
                    break;
                }
            }

            query_position += view_direction * 0.1f;
        }

        return result;
    }

    void load_chunks_at_region(World *world, const World_Region_Bounds& region_bounds)
    {
        PROFILE_FUNCTION;

        for (i32 z = region_bounds.min.y - 1; z <= region_bounds.max.y + 1; ++z)
        {
            for (i32 x = region_bounds.min.x - 1; x <= region_bounds.max.x + 1; ++x)
            {
                glm::ivec2 chunk_coords = { x, z };
                auto it = world->loaded_chunks.find(chunk_coords);

                if (it == world->loaded_chunks.end())
                {
                    Chunk* chunk = world->chunk_pool.allocate();
                    initialize_chunk(chunk, chunk_coords, world->path);
                    world->loaded_chunks.emplace(chunk_coords, chunk);
                    Load_Chunk_Job load_chunk_job;
                    load_chunk_job.world = world;
                    load_chunk_job.chunk = chunk;
                    Job_System::schedule(load_chunk_job);
                }
            }
        }

        auto dirs = get_chunk_neighbour_directions();

        for (i32 z = region_bounds.min.y; z <= region_bounds.max.y; ++z)
        {
            for (i32 x = region_bounds.min.x; x <= region_bounds.max.x; ++x)
            {
                glm::ivec2 chunk_coords = { x, z };
                auto it = world->loaded_chunks.find(chunk_coords);
                assert(it != world->loaded_chunks.end());
                Chunk *chunk = it->second;
                bool all_neighbours_loaded = true;
                for (i32 i = 0; i < ChunkNeighbour_Count; i++)
                {
                    glm::ivec2 neighbour_chunk_coords = chunk_coords + dirs[i];
                    auto neighbour_it = world->loaded_chunks.find(neighbour_chunk_coords);
                    assert(neighbour_it != world->loaded_chunks.end());
                    Chunk *neighbour_chunk = neighbour_it->second;
                    chunk->neighbours[i] = neighbour_chunk;
                    if (!neighbour_chunk->loaded)
                    {
                        all_neighbours_loaded = false;
                    }
                }

                if (all_neighbours_loaded && !chunk->neighbours_loaded)
                {
                    chunk->neighbours_loaded = true;
                }
            }
        }
    }

    void free_chunks_out_of_region(World *world, const World_Region_Bounds& region_bounds)
    {
        PROFILE_FUNCTION;

        auto& free_chunks = world->pending_free_chunks;

        for (auto it = world->loaded_chunks.begin(); it != world->loaded_chunks.end();)
        {
            auto chunk_coords = it->first;
            Chunk* chunk = it->second;

            // todo(harlequin): parameterize out of bounds
            bool out_of_bounds = chunk_coords.x < region_bounds.min.x - 2 ||
                                 chunk_coords.x > region_bounds.max.x + 2 ||
                                 chunk_coords.y < region_bounds.min.y - 2 ||
                                 chunk_coords.y > region_bounds.max.y + 2;

            if (out_of_bounds &&
                !chunk->pending_for_load &&
                chunk->loaded &&
                !chunk->pending_for_lighting &&
                !chunk->in_light_propagation_queue &&
                !chunk->in_light_calculation_queue &&
                !chunk->pending_for_update &&
                !chunk->pending_for_save)
            {
                chunk->pending_for_save = true;

                Serialize_And_Free_Chunk_Job serialize_and_free_chunk_job;
                serialize_and_free_chunk_job.world = world;
                serialize_and_free_chunk_job.chunk = chunk;
                Job_System::schedule(serialize_and_free_chunk_job, false);

                free_chunks.emplace_back(chunk);
                it = world->loaded_chunks.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = free_chunks.begin(); it != free_chunks.end();)
        {
            Chunk* chunk = *it;

            if (chunk->unload)
            {
                world->chunk_pool.reclame(chunk);
                it = free_chunks.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    static void queue_update_sub_chunk_job(Circular_FIFO_Queue< Update_Chunk_Job > &queue, Chunk *chunk, i32 sub_chunk_index)
    {
        Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];

        if (!render_data.pending_for_update)
        {
            render_data.pending_for_update = true;

            if (!chunk->pending_for_update)
            {
                chunk->pending_for_update = true;

                Update_Chunk_Job job;
                job.chunk = chunk;
                queue.push(job);
            }
        }
    }

    void set_block_id(Chunk *chunk, const glm::ivec3& block_coords, u16 block_id)
    {
        PROFILE_FUNCTION;

        chunk->pending_for_lighting = true;
        chunk->light_propagated = false;
        chunk->light_calculated = false;

        for (i32 i = 0; i < ChunkNeighbour_Count; i++)
        {
            Chunk* neighbour = chunk->neighbours[i];
            assert(neighbour);
            neighbour->pending_for_lighting = true;
            neighbour->light_propagated = false;
            neighbour->light_calculated = false;
        }

        Block *block = get_block(chunk, block_coords);
        block->id = block_id;

        i32 sub_chunk_index = get_sub_chunk_index(block_coords);

        if (block_coords.x == 0)
        {
            Chunk *left_chunk = chunk->neighbours[ChunkNeighbour_Left];
            assert(left_chunk);
            left_chunk->right_edge_blocks[block_coords.y * MC_CHUNK_DEPTH + block_coords.z].id = block_id;
        }
        else if (block_coords.x == MC_CHUNK_WIDTH - 1)
        {
            Chunk *right_chunk = chunk->neighbours[ChunkNeighbour_Right];
            assert(right_chunk);
            right_chunk->left_edge_blocks[block_coords.y * MC_CHUNK_DEPTH + block_coords.z].id = block_id;
        }

        if (block_coords.z == 0)
        {
            Chunk *front_chunk = chunk->neighbours[ChunkNeighbour_Front];
            assert(front_chunk);
            front_chunk->back_edge_blocks[block_coords.y * MC_CHUNK_WIDTH + block_coords.x].id = block_id;
        }
        else if (block_coords.z == MC_CHUNK_DEPTH - 1)
        {
            Chunk *back_chunk = chunk->neighbours[ChunkNeighbour_Back];
            assert(back_chunk);
            back_chunk->front_edge_blocks[block_coords.y * MC_CHUNK_WIDTH + block_coords.x].id = block_id;
        }
    }

    // todo(harlequin): remove *world
    void set_block_sky_light_level(World *world, Chunk *chunk, const glm::ivec3& block_coords, u8 light_level)
    {
        Block *block = get_block(chunk, block_coords);
        Block_Light_Info *light_info = get_block_light_info(chunk, block_coords);
        light_info->sky_light_level = light_level;

        i32 sub_chunk_index = get_sub_chunk_index(block_coords);
        queue_update_sub_chunk_job(world->update_chunk_jobs_queue, chunk, sub_chunk_index);

        if (block_coords.x == 0)
        {
            Chunk *left_chunk = chunk->neighbours[ChunkNeighbour_Left];
            assert(left_chunk);
            left_chunk->right_edge_light_map[block_coords.y * MC_CHUNK_DEPTH + block_coords.z].sky_light_level = light_level;
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, left_chunk, sub_chunk_index);
        }
        else if (block_coords.x == MC_CHUNK_WIDTH - 1)
        {
            Chunk *right_chunk = chunk->neighbours[ChunkNeighbour_Right];
            assert(right_chunk);
            right_chunk->left_edge_light_map[block_coords.y * MC_CHUNK_DEPTH + block_coords.z].sky_light_level = light_level;
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, right_chunk, sub_chunk_index);
        }

        if (block_coords.z == 0)
        {
            Chunk *front_chunk = chunk->neighbours[ChunkNeighbour_Front];
            assert(front_chunk);
            front_chunk->back_edge_light_map[block_coords.y * MC_CHUNK_WIDTH + block_coords.x].sky_light_level = light_level;
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, front_chunk, sub_chunk_index);
        }
        else if (block_coords.z == MC_CHUNK_DEPTH - 1)
        {
            Chunk *back_chunk = chunk->neighbours[ChunkNeighbour_Back];
            assert(back_chunk);
            back_chunk->front_edge_light_map[block_coords.y * MC_CHUNK_WIDTH + block_coords.x].sky_light_level = light_level;
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, back_chunk, sub_chunk_index);
        }

        i32 sub_chunk_start_y = sub_chunk_index * World::sub_chunk_height;
        i32 sub_chunk_end_y = (sub_chunk_index + 1) * World::sub_chunk_height - 1;

        if (block_coords.y == sub_chunk_end_y && sub_chunk_index != World::sub_chunk_count_per_chunk - 1)
        {
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, chunk, sub_chunk_index + 1);
        }
        else if (block_coords.y == sub_chunk_start_y && sub_chunk_index != 0)
        {
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, chunk, sub_chunk_index - 1);
        }
    }

    // todo(harlequin): remove *world
    void set_block_light_source_level(World *world, Chunk *chunk, const glm::ivec3& block_coords, u8 light_level)
    {
        Block *block = get_block(chunk, block_coords);
        Block_Light_Info *light_info = get_block_light_info(chunk, block_coords);
        light_info->light_source_level = light_level;

        i32 sub_chunk_index = get_sub_chunk_index(block_coords);
        queue_update_sub_chunk_job(world->update_chunk_jobs_queue, chunk, sub_chunk_index);

        if (block_coords.x == 0)
        {
            Chunk *left_chunk = chunk->neighbours[ChunkNeighbour_Left];
            assert(left_chunk);
            left_chunk->right_edge_light_map[block_coords.y * MC_CHUNK_DEPTH + block_coords.z].light_source_level = light_level;
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, left_chunk, sub_chunk_index);
        }
        else if (block_coords.x == MC_CHUNK_WIDTH - 1)
        {
            Chunk *right_chunk = chunk->neighbours[ChunkNeighbour_Right];
            assert(right_chunk);
            right_chunk->left_edge_light_map[block_coords.y * MC_CHUNK_DEPTH + block_coords.z].light_source_level = light_level;
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, right_chunk, sub_chunk_index);
        }

        if (block_coords.z == 0)
        {
            Chunk *front_chunk = chunk->neighbours[ChunkNeighbour_Front];
            assert(front_chunk);
            front_chunk->back_edge_light_map[block_coords.y * MC_CHUNK_WIDTH + block_coords.x].light_source_level = light_level;
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, front_chunk, sub_chunk_index);
        }
        else if (block_coords.z == MC_CHUNK_DEPTH - 1)
        {
            Chunk *back_chunk = chunk->neighbours[ChunkNeighbour_Back];
            assert(back_chunk);
            back_chunk->front_edge_light_map[block_coords.y * MC_CHUNK_WIDTH + block_coords.x].light_source_level = light_level;
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, back_chunk, sub_chunk_index);
        }

        i32 sub_chunk_start_y = sub_chunk_index * World::sub_chunk_height;
        i32 sub_chunk_end_y = (sub_chunk_index + 1) * World::sub_chunk_height - 1;

        if (block_coords.y == sub_chunk_end_y && sub_chunk_index != World::sub_chunk_count_per_chunk - 1)
        {
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, chunk, sub_chunk_index + 1);
        }
        else if (block_coords.y == sub_chunk_start_y && sub_chunk_index != 0)
        {
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, chunk, sub_chunk_index - 1);
        }
    }

    Chunk* get_chunk(World *world, const glm::ivec2& chunk_coords)
    {
        auto it = world->loaded_chunks.find({ chunk_coords.x, chunk_coords.y });
        Chunk *chunk = nullptr;
        if (it != world->loaded_chunks.end())
        {
            chunk = it->second;
        }
        return chunk;
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
            if (block_coords.x >= 0 && block_coords.x < MC_CHUNK_WIDTH &&
                block_coords.y >= 0 && block_coords.y < MC_CHUNK_HEIGHT &&
                block_coords.z >= 0 && block_coords.z < MC_CHUNK_WIDTH)
            {
                return { block_coords, get_block(chunk, block_coords), chunk };
            }
        }
        return { { -1, -1, -1 }, &World::null_block, nullptr };
    }

    Block_Query_Result world_get_neighbour_block_from_top(Chunk *chunk, const glm::ivec3& block_coords)
    {
        Block_Query_Result result;
        result.block_coords = { block_coords.x, block_coords.y + 1, block_coords.z };
        result.chunk = chunk;
        result.block = get_neighbour_block_from_top(chunk, block_coords);
        return result;
    }

    Block_Query_Result world_get_neighbour_block_from_bottom(Chunk *chunk, const glm::ivec3& block_coords)
    {
        Block_Query_Result result;
        result.block_coords = { block_coords.x, block_coords.y - 1, block_coords.z };
        result.chunk = chunk;
        result.block = get_neighbour_block_from_bottom(result.chunk, block_coords);
        return result;
    }

    Block_Query_Result world_get_neighbour_block_from_left(Chunk *chunk, const glm::ivec3& block_coords)
    {
        Block_Query_Result result;

        if (block_coords.x == 0)
        {
            result.chunk = chunk->neighbours[ChunkNeighbour_Left];
            result.block_coords = { MC_CHUNK_WIDTH - 1, block_coords.y, block_coords.z };
        }
        else
        {
            result.chunk = chunk;
            result.block_coords = { block_coords.x - 1, block_coords.y, block_coords.z };
        }

        result.block = get_block(result.chunk, result.block_coords);
        return result;
    }

    Block_Query_Result world_get_neighbour_block_from_right(Chunk *chunk, const glm::ivec3& block_coords)
    {
        Block_Query_Result result;

        if (block_coords.x == MC_CHUNK_WIDTH - 1)
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

    Block_Query_Result world_get_neighbour_block_from_front(Chunk *chunk, const glm::ivec3& block_coords)
    {
        Block_Query_Result result;

        if (block_coords.z == 0)
        {
            result.chunk = chunk->neighbours[ChunkNeighbour_Front];
            result.block_coords = { block_coords.x, block_coords.y, MC_CHUNK_DEPTH - 1 };
        }
        else
        {
            result.chunk = chunk;
            result.block_coords = { block_coords.x, block_coords.y, block_coords.z - 1 };
        }

        result.block = get_block(result.chunk, result.block_coords);
        return result;
    }

    Block_Query_Result world_get_neighbour_block_from_back(Chunk *chunk, const glm::ivec3& block_coords)
    {
        Block_Query_Result result;
        if (block_coords.z == MC_CHUNK_DEPTH - 1)
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

    void schedule_chunk_lighting_jobs(World *world, World_Region_Bounds *player_region_bounds)
    {
        PROFILE_FUNCTION;

        auto& light_propagation_queue        = world->light_propagation_queue;
        auto& calculate_chunk_lighting_queue = world->calculate_chunk_lighting_queue;

        auto dirs = get_chunk_neighbour_directions();

        for (i32 z = player_region_bounds->min.y; z <= player_region_bounds->max.y; ++z)
        {
            for (i32 x = player_region_bounds->min.x; x <= player_region_bounds->max.x; ++x)
            {
                glm::ivec2 chunk_coords = { x, z };
                auto it = world->loaded_chunks.find(chunk_coords);
                assert(it != world->loaded_chunks.end());
                Chunk* chunk = it->second;

                if (chunk->loaded &&
                    chunk->neighbours_loaded)
                {
                    if (chunk->pending_for_lighting && !chunk->light_propagated && !chunk->in_light_propagation_queue)
                    {
                        chunk->pending_for_lighting = false;
                        chunk->in_light_propagation_queue = true;

                        Calculate_Chunk_Light_Propagation_Job job;
                        job.world = world;
                        job.chunk = chunk;
                        light_propagation_queue.push(job);
                    }

                    if (chunk->light_propagated)
                    {
                        bool all_neighbours_light_propagated = true;

                        for (i32 i = 0; i < ChunkNeighbour_Count; i++)
                        {
                            Chunk *neighbour_chunk = chunk->neighbours[i];

                            if (!neighbour_chunk->light_propagated)
                            {
                                all_neighbours_light_propagated = false;
                                break;
                            }
                        }

                        if (all_neighbours_light_propagated && !chunk->light_calculated)
                        {
                            chunk->light_calculated = true;
                            chunk->in_light_calculation_queue = true;

                            Calculate_Chunk_Lighting_Job job;
                            job.world = world;
                            job.chunk = chunk;
                            calculate_chunk_lighting_queue.push(job);
                        }
                    }
                }
            }
        }
    }

    void schedule_save_chunks_jobs(World *world)
    {
        for (auto it = world->loaded_chunks.begin(); it != world->loaded_chunks.end(); ++it)
        {
            Chunk *chunk = it->second;
            chunk->pending_for_save = true;

            Serialize_Chunk_Job job;
            job.world = world;
            job.chunk = chunk;
            Job_System::schedule(job);
        }
    }

    std::array<Block_Query_Result, 6> world_get_neighbours(Chunk *chunk, const glm::ivec3& block_coords)
    {
        std::array<Block_Query_Result, 6> neighbours;
        neighbours[BlockNeighbour_Up]    = world_get_neighbour_block_from_top(chunk,    block_coords);
        neighbours[BlockNeighbour_Down]  = world_get_neighbour_block_from_bottom(chunk, block_coords);
        neighbours[BlockNeighbour_Left]  = world_get_neighbour_block_from_left(chunk,   block_coords);
        neighbours[BlockNeighbour_Right] = world_get_neighbour_block_from_right(chunk,  block_coords);
        neighbours[BlockNeighbour_Front] = world_get_neighbour_block_from_front(chunk,  block_coords);
        neighbours[BlockNeighbour_Back]  = world_get_neighbour_block_from_back(chunk,   block_coords);
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
            BlockFlags_Is_Transparent
        },
        // Grass
        {
            "grass",
            Texture_Id_grass_block_top,
            Texture_Id_dirt,
            Texture_Id_grass_block_side,
            BlockFlags_Is_Solid | BlockFlags_Should_Color_Top_By_Biome
        },
        // Sand
        {
            "sand",
            Texture_Id_sand,
            Texture_Id_sand,
            Texture_Id_sand,
            BlockFlags_Is_Solid
        },
        // Dirt
        {
            "dirt",
            Texture_Id_dirt,
            Texture_Id_dirt,
            Texture_Id_dirt,
            BlockFlags_Is_Solid
        },
        // Stone
        {
            "stone",
            Texture_Id_stone,
            Texture_Id_stone,
            Texture_Id_stone,
            BlockFlags_Is_Solid
        },
        // Green Concrete
        {
            "green_concrete",
            Texture_Id_green_concrete_powder,
            Texture_Id_green_concrete_powder,
            Texture_Id_green_concrete_powder,
            BlockFlags_Is_Solid
        },
        // BedRock
        {
            "bedrock",
            Texture_Id_bedrock,
            Texture_Id_bedrock,
            Texture_Id_bedrock,
            BlockFlags_Is_Solid,
        },
        // Oak Log
        {
            "oak_log",
            Texture_Id_oak_log_top,
            Texture_Id_oak_log_top,
            Texture_Id_oak_log,
            BlockFlags_Is_Solid,
        },
        // Oak Leaves
        {
            "oak_leaves",
            Texture_Id_oak_leaves,
            Texture_Id_oak_leaves,
            Texture_Id_oak_leaves,
            BlockFlags_Is_Solid |
            BlockFlags_Is_Transparent |
            BlockFlags_Should_Color_Top_By_Biome |
            BlockFlags_Should_Color_Side_By_Biome |
            BlockFlags_Should_Color_Bottom_By_Biome
        },
        // Oak Planks
        {
            "oak_planks",
            Texture_Id_oak_planks,
            Texture_Id_oak_planks,
            Texture_Id_oak_planks,
            BlockFlags_Is_Solid
        },
        // Glow Stone
        {
            "glow_stone",
            Texture_Id_glowstone,
            Texture_Id_glowstone,
            Texture_Id_glowstone,
            BlockFlags_Is_Solid | BlockFlags_Is_Light_Source
        },
        // Cobble Stone
        {
            "cobble_stone",
            Texture_Id_cobblestone,
            Texture_Id_cobblestone,
            Texture_Id_cobblestone,
            BlockFlags_Is_Solid
        },
        // Spruce Log
        {
            "spruce_log",
            Texture_Id_spruce_log_top,
            Texture_Id_spruce_log_top,
            Texture_Id_spruce_log,
            BlockFlags_Is_Solid
        },
        // Spruce Planks
        {
            "spruce_planks",
            Texture_Id_spruce_planks,
            Texture_Id_spruce_planks,
            Texture_Id_spruce_planks,
            BlockFlags_Is_Solid
        },
        // Glass
        {
            "glass",
            Texture_Id_glass,
            Texture_Id_glass,
            Texture_Id_glass,
            BlockFlags_Is_Solid | BlockFlags_Is_Transparent
        },
        // Sea Lantern
        {
            "sea lantern",
            Texture_Id_sea_lantern,
            Texture_Id_sea_lantern,
            Texture_Id_sea_lantern,
            BlockFlags_Is_Solid
        },
        // Birch Log
        {
            "birch log",
            Texture_Id_birch_log_top,
            Texture_Id_birch_log_top,
            Texture_Id_birch_log,
            BlockFlags_Is_Solid
        },
        // Blue Stained Glass
        {
            "blue stained glass",
            Texture_Id_blue_stained_glass,
            Texture_Id_blue_stained_glass,
            Texture_Id_blue_stained_glass,
            BlockFlags_Is_Solid | BlockFlags_Is_Transparent,
        },
        // Water
        {
            "water",
            Texture_Id_water,
            Texture_Id_water,
            Texture_Id_water,
            BlockFlags_Is_Transparent,
        },
        // Birch Planks
        {
            "birch_planks",
            Texture_Id_birch_planks,
            Texture_Id_birch_planks,
            Texture_Id_birch_planks,
            BlockFlags_Is_Solid
        },
        // Diamond
        {
            "diamond",
            Texture_Id_diamond_block,
            Texture_Id_diamond_block,
            Texture_Id_diamond_block,
            BlockFlags_Is_Solid
        },
        // Obsidian
        {
            "obsidian",
            Texture_Id_obsidian,
            Texture_Id_obsidian,
            Texture_Id_obsidian,
            BlockFlags_Is_Solid
        },
        // Crying Obsidian
        {
            "crying_obsidian",
            Texture_Id_crying_obsidian,
            Texture_Id_crying_obsidian,
            Texture_Id_crying_obsidian,
            BlockFlags_Is_Solid
        },
        // Dark Oak Log
        {
            "dark_oak_log",
            Texture_Id_dark_oak_log_top,
            Texture_Id_dark_oak_log_top,
            Texture_Id_dark_oak_log,
            BlockFlags_Is_Solid
        },
        // Dark Oak Planks
        {
            "dark_oak_planks",
            Texture_Id_dark_oak_planks,
            Texture_Id_dark_oak_planks,
            Texture_Id_dark_oak_planks,
            BlockFlags_Is_Solid
        },
        // Jungle Log
        {
            "jungle_log",
            Texture_Id_jungle_log_top,
            Texture_Id_jungle_log_top,
            Texture_Id_jungle_log,
            BlockFlags_Is_Solid
        },
        // Jungle Planks
        {
            "jungle_planks",
            Texture_Id_jungle_planks,
            Texture_Id_jungle_planks,
            Texture_Id_jungle_planks,
            BlockFlags_Is_Solid
        },
        // Acacia Log
        {
            "acacia_log",
            Texture_Id_acacia_log_top,
            Texture_Id_acacia_log_top,
            Texture_Id_acacia_log,
            BlockFlags_Is_Solid
        },
        // Acacia Planks
        {
            "acacia_planks",
            Texture_Id_acacia_planks,
            Texture_Id_acacia_planks,
            Texture_Id_acacia_planks,
            BlockFlags_Is_Solid
        }
    };

    Block World::null_block = { BlockId_Air };

}