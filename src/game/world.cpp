#include "world.h"
#include "core/file_system.h"
#include "renderer/opengl_renderer.h"
#include "game/job_system.h"
#include "game/jobs.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/noise.hpp>
#include <glm/gtx/compatibility.hpp>

#include <time.h>
#include <errno.h>

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
        return block_coords.y * CHUNK_WIDTH * CHUNK_DEPTH + block_coords.z * CHUNK_WIDTH + block_coords.x;
    }

    glm::vec3 get_block_position(Chunk *chunk, const glm::ivec3& block_coords)
    {
        return chunk->position + glm::vec3((f32)block_coords.x + 0.5f, (f32)block_coords.y + 0.5f, (f32)block_coords.z + 0.5f);
    }

    Block* get_block(Chunk *chunk, const glm::ivec3& block_coords)
    {
        Assert(block_coords.x >= 0 && block_coords.x < CHUNK_WIDTH &&
               block_coords.y >= 0 && block_coords.y < CHUNK_HEIGHT &&
               block_coords.z >= 0 && block_coords.z < CHUNK_DEPTH);

        i32 block_index = get_block_index(block_coords);
        return chunk->blocks + block_index;
    }

    Block_Light_Info* get_block_light_info(Chunk *chunk, const glm::ivec3& block_coords)
    {
        Assert(block_coords.x >= 0 && block_coords.x < CHUNK_WIDTH &&
               block_coords.y >= 0 && block_coords.y < CHUNK_HEIGHT &&
               block_coords.z >= 0 && block_coords.z < CHUNK_DEPTH);
        i32 block_index = get_block_index(block_coords);
        return chunk->light_map + block_index;
    }

    glm::ivec2 world_position_to_chunk_coords(const glm::vec3& position)
    {
        const f32 one_over_16 = 1.0f / 16.0f;
        return { glm::floor(position.x * one_over_16), glm::floor(position.z * one_over_16) };
    }

    bool initialize_chunk(Chunk *chunk, const glm::ivec2& world_coords)
    {
        chunk->world_coords = world_coords;
        chunk->position = { world_coords.x * CHUNK_WIDTH, 0.0f, world_coords.y * CHUNK_DEPTH };

        for (i32 sub_chunk_index = 0; sub_chunk_index < World::sub_chunk_count_per_chunk; ++sub_chunk_index)
        {
            Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];

            render_data.face_count      = 0;
            render_data.bucket_index    = 0;
            render_data.instance_memory_id = -1;

            for (i32 j = 0; j < 2; j++)
            {
                render_data.opaque_buckets[j].memory_id       = -1;
                render_data.opaque_buckets[j].current_vertex = nullptr;
                render_data.opaque_buckets[j].face_count     = 0;

                render_data.transparent_buckets[j].memory_id      = -1;
                render_data.transparent_buckets[j].current_vertex = nullptr;
                render_data.transparent_buckets[j].face_count     = 0;

                constexpr f32 infinity = std::numeric_limits<f32>::max();
                render_data.aabb[j] = { { infinity, infinity, infinity }, { -infinity, -infinity, -infinity } };
            }

            render_data.uploaded_to_gpu    = false;
            render_data.pending_for_update = false;
        }

        chunk->pending_for_tessellation = false;
        chunk->tessellated              = false;
        chunk->state                    = ChunkState_Initialized;

        for (i32 i = 0; i < ChunkNeighbour_Count; i++)
        {
            chunk->neighbours[i] = nullptr;
        }

        return true;
    }

    inline static glm::vec2 get_sample(i32 seed, const glm::ivec2& chunk_coords, const glm::ivec2& block_xz_coords)
    {
        return {
            seed + chunk_coords.x * CHUNK_WIDTH + block_xz_coords.x + 0.5f,
            seed + chunk_coords.y * CHUNK_DEPTH + block_xz_coords.y + 0.5f };
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
        i32 height_map[CHUNK_DEPTH][CHUNK_WIDTH];

        i32 top_edge_height_map[CHUNK_WIDTH];
        i32 bottom_edge_height_map[CHUNK_WIDTH];
        i32 left_edge_height_map[CHUNK_DEPTH];
        i32 right_edge_height_map[CHUNK_DEPTH];

        constexpr i32 min_biome_height = 100;
        constexpr i32 max_biome_height = 250;
        constexpr i32 water_level = min_biome_height + 50;
        static_assert(water_level >= min_biome_height && water_level <= max_biome_height);

        const glm::ivec2 front_chunk_coords = { chunk->world_coords.x + 0, chunk->world_coords.y - 1 };
        const glm::ivec2 back_chunk_coords  = { chunk->world_coords.x + 0, chunk->world_coords.y + 1 };
        const glm::ivec2 left_chunk_coords  = { chunk->world_coords.x - 1, chunk->world_coords.y + 0 };
        const glm::ivec2 right_chunk_coords = { chunk->world_coords.x + 1, chunk->world_coords.y + 0 };

        for (i32 z = 0; z < CHUNK_DEPTH; ++z)
        {
            for (i32 x = 0; x < CHUNK_WIDTH; ++x)
            {
                glm::ivec2 block_xz_coords = { x, z };
                glm::vec2 sample = get_sample(seed, chunk->world_coords, block_xz_coords);
                f32 noise = get_noise01(sample);
                assert(noise >= 0.0f && noise <= 1.0f);
                height_map[z][x] = get_height_from_noise01(min_biome_height, max_biome_height, noise);
            }
        }

        for (i32 x = 0; x < CHUNK_WIDTH; ++x)
        {
            glm::ivec2 block_xz_coords = { x, CHUNK_DEPTH - 1 };
            glm::vec2 sample = get_sample(seed, front_chunk_coords, block_xz_coords);
            f32 noise = get_noise01(sample);
            top_edge_height_map[x] = get_height_from_noise01(min_biome_height, max_biome_height, noise);
        }

        for (i32 x = 0; x < CHUNK_WIDTH; ++x)
        {
            glm::ivec2 block_xz_coords = { x, 0 };
            glm::vec2 sample = get_sample(seed, back_chunk_coords, block_xz_coords);
            f32 noise = get_noise01(sample);
            bottom_edge_height_map[x] = get_height_from_noise01(min_biome_height, max_biome_height, noise);
        }

        for (i32 z = 0; z < CHUNK_DEPTH; ++z)
        {
            glm::ivec2 block_xz_coords = { CHUNK_WIDTH - 1, z };
            glm::vec2 sample = get_sample(seed, left_chunk_coords, block_xz_coords);
            f32 noise = get_noise01(sample);
            left_edge_height_map[z] = get_height_from_noise01(min_biome_height, max_biome_height, noise);
        }

        for (i32 z = 0; z < CHUNK_DEPTH; ++z)
        {
            glm::ivec2 block_xz_coords = { 0, z };
            glm::vec2 sample = get_sample(seed, right_chunk_coords, block_xz_coords);
            f32 noise = get_noise01(sample);
            right_edge_height_map[z] = get_height_from_noise01(min_biome_height, max_biome_height, noise);
        }

        for (i32 y = 0; y < CHUNK_HEIGHT; ++y)
        {
            for (i32 z = 0; z < CHUNK_DEPTH; ++z)
            {
                for (i32 x = 0; x < CHUNK_WIDTH; ++x)
                {
                    glm::ivec3 block_coords = { x, y, z };
                    Block *block = get_block(chunk, block_coords);
                    const i32& height = height_map[z][x];
                    set_block_id_based_on_height(block, y, height, min_biome_height, max_biome_height, water_level);
                }
            }
        }

        for (i32 y = 0; y < CHUNK_HEIGHT; ++y)
        {
            for (i32 x = 0; x < CHUNK_WIDTH; ++x)
            {
                const i32& top_edge_height = top_edge_height_map[x];
                Block *top_edge_block = &(chunk->front_edge_blocks[y * CHUNK_WIDTH + x]);
                set_block_id_based_on_height(top_edge_block, y, top_edge_height, min_biome_height, max_biome_height, water_level);

                const i32& bottom_edge_height = bottom_edge_height_map[x];
                Block *bottom_edge_block = &(chunk->back_edge_blocks[y * CHUNK_WIDTH + x]);
                set_block_id_based_on_height(bottom_edge_block, y, bottom_edge_height, min_biome_height, max_biome_height, water_level);
            }

            for (i32 z = 0; z < CHUNK_DEPTH; ++z)
            {
                const i32& left_edge_height = left_edge_height_map[z];
                Block *left_edge_block = &(chunk->left_edge_blocks[y * CHUNK_DEPTH + z]);
                set_block_id_based_on_height(left_edge_block, y, left_edge_height, min_biome_height, max_biome_height, water_level);

                const i32& right_edge_height = right_edge_height_map[z];
                Block *right_edge_block = &(chunk->right_edge_blocks[y * CHUNK_DEPTH + z]);
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

    void serialize_chunk(World *world,
                         Chunk *chunk,
                         i32 seed,
                         Temprary_Memory_Arena *temp_arena)
    {
        Assert(chunk->state >= ChunkState_Loaded);

        Chunk *original_chunk = ArenaPushZero(temp_arena, Chunk);
        initialize_chunk(original_chunk, chunk->world_coords);
        generate_chunk(original_chunk, seed);

        u32 block_count = 0;
        Block_Serialization_Info serialized_blocks[CHUNK_HEIGHT * CHUNK_DEPTH * CHUNK_WIDTH];

        for (i32 block_index = 0; block_index < CHUNK_HEIGHT * CHUNK_DEPTH * CHUNK_WIDTH; block_index++)
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
        Block_Serialization_Info serialized_front_edge_blocks[CHUNK_HEIGHT * CHUNK_WIDTH];

        for (u32 block_index = 0; block_index < CHUNK_HEIGHT * CHUNK_WIDTH; block_index++)
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
        Block_Serialization_Info serialized_back_edge_blocks[CHUNK_HEIGHT  * CHUNK_WIDTH];

        for (u32 block_index = 0; block_index < CHUNK_HEIGHT * CHUNK_WIDTH; block_index++)
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
        Block_Serialization_Info serialized_left_edge_blocks[CHUNK_HEIGHT * CHUNK_DEPTH];

        for (u32 block_index = 0; block_index < CHUNK_HEIGHT * CHUNK_WIDTH; block_index++)
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
        Block_Serialization_Info serialized_right_edge_blocks[CHUNK_HEIGHT * CHUNK_DEPTH];

        for (u32 block_index = 0; block_index < CHUNK_HEIGHT * CHUNK_WIDTH; block_index++)
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

        String8 chunk_file_path = get_chunk_file_path(world, chunk, temp_arena);

        u32 serialized_block_count = block_count + front_edge_count + back_edge_count + left_edge_count + right_edge_count;

        if (serialized_block_count)
        {
            FILE *file = fopen(chunk_file_path.data, "wb");

            if (file == NULL)
            {
                fprintf(stderr,
                        "[ERROR]: failed to open file %.*s for writing: %s\n",
                        (i32)chunk_file_path.count, chunk_file_path.data, strerror(errno));
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
            if (File_System::exists(chunk_file_path.data))
            {
                if (!File_System::delete_file(chunk_file_path.data))
                {
                    fprintf(stderr,
                            "[ERROR]: failed to delete file: %.*s\n",
                            (i32)chunk_file_path.count,
                            chunk_file_path.data);
                }
            }
        }
    }

    void deserialize_chunk(World *world,
                           Chunk *chunk,
                           Temprary_Memory_Arena *temp_arena)
    {
        Assert(chunk->state == ChunkState_Initialized);
        String8 chunk_file_path = get_chunk_file_path(world, chunk, temp_arena);
        FILE *file = fopen(chunk_file_path.data, "rb");
        if (file == NULL)
        {
            fprintf(stderr,
                    "[ERROR]: failed to open file %.*s for reading: %s\n",
                    (i32)chunk_file_path.count,
                    chunk_file_path.data,
                    strerror(errno));
            return;
        }

        Chunk_Serialization_Header header;
        fread(&header, sizeof(Chunk_Serialization_Header), 1, file);

        if (header.block_count)
        {
            Block_Serialization_Info serialized_blocks[CHUNK_HEIGHT * CHUNK_DEPTH * CHUNK_WIDTH];
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
            Block_Serialization_Info serialized_front_edge_blocks[CHUNK_HEIGHT * CHUNK_WIDTH];
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
            Block_Serialization_Info serialized_back_edge_blocks[CHUNK_HEIGHT * CHUNK_WIDTH];
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
            Block_Serialization_Info serialized_left_edge_blocks[CHUNK_HEIGHT * CHUNK_DEPTH];
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
            Block_Serialization_Info serialized_right_edge_blocks[CHUNK_HEIGHT * CHUNK_DEPTH];
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
        if (block_coords.x == CHUNK_WIDTH - 1)
        {
            return &(chunk->right_edge_blocks[block_coords.y * CHUNK_DEPTH + block_coords.z]);
        }

        return get_block(chunk, { block_coords.x + 1, block_coords.y, block_coords.z });
    }

    Block* get_neighbour_block_from_left(Chunk *chunk, const glm::ivec3& block_coords)
    {
        if (block_coords.x == 0)
        {
            return &(chunk->left_edge_blocks[block_coords.y * CHUNK_DEPTH + block_coords.z]);
        }

        return get_block(chunk, { block_coords.x - 1, block_coords.y, block_coords.z });
    }

    Block* get_neighbour_block_from_top(Chunk *chunk, const glm::ivec3& block_coords)
    {
        if (block_coords.y == CHUNK_HEIGHT - 1)
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
            return &(chunk->front_edge_blocks[block_coords.y * CHUNK_WIDTH + block_coords.x]);
        }
        return get_block(chunk, { block_coords.x, block_coords.y, block_coords.z - 1 });
    }

    Block* get_neighbour_block_from_back(Chunk *chunk, const glm::ivec3& block_coords)
    {
        if (block_coords.z == CHUNK_DEPTH - 1)
        {
            return &(chunk->back_edge_blocks[block_coords.y * CHUNK_WIDTH + block_coords.x]);
        }

        return get_block(chunk, { block_coords.x, block_coords.y, block_coords.z + 1 });
    }

    std::array< Block*, 6 > get_neighbours(Chunk *chunk, const glm::ivec3& block_coords)
    {
        std::array< Block*, 6 > neighbours;

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
        for (i32 z = 0; z < CHUNK_DEPTH; z++)
        {
            for (i32 x = 0; x < CHUNK_WIDTH; x++)
            {
                bool can_sky_light_propagate = true;

                for (i32 y = CHUNK_HEIGHT - 1; y >= 0; y--)
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

    void calculate_lighting(World *world,
                            Chunk *chunk,
                            Circular_FIFO_Queue< Block_Query_Result > *queue)
    {
        for (i32 y = CHUNK_HEIGHT - 1; y >= 0; y--)
        {
            bool found_any_sky_lights = false;
            for (i32 z = 0; z < CHUNK_WIDTH; z++)
            {
                for (i32 x = 0; x < CHUNK_WIDTH; x++)
                {
                    glm::ivec3 block_coords = { x, y, z };
                    Block* block = get_block(chunk, block_coords);
                    const Block_Info* info = get_block_info(world, block);
                    if (!is_block_transparent(info)) continue;
                    Block_Light_Info *block_light_info = get_block_light_info(chunk, block_coords);
                    if (block_light_info->sky_light_level == 15)
                    {
                        found_any_sky_lights = true;
                        auto neighbours_query = query_neighbours(chunk, block_coords);
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
            if (!found_any_sky_lights) break;
        }
    }

    glm::ivec3 world_position_to_block_coords(World *world, const glm::vec3& position)
    {
        glm::ivec2 chunk_coords  = world_position_to_chunk_coords(position);
        glm::vec3 chunk_position = { chunk_coords.x * CHUNK_WIDTH, 0.0f, chunk_coords.y * CHUNK_DEPTH };
        glm::vec3 offset         = position - chunk_position;
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
               query.block_coords.x >= 0 && query.block_coords.x < CHUNK_WIDTH  &&
               query.block_coords.y >= 0 && query.block_coords.y < CHUNK_HEIGHT &&
               query.block_coords.z >= 0 && query.block_coords.z < CHUNK_DEPTH;
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

        if (!File_System::exists(world_path.data))
        {
            File_System::create_directory(world_path.data);
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

        for (u32 i = 0; i < World::chunk_capacity; i++)
        {
            world->chunk_hash_table[i].chunk_node_index = INVALID_CHUNK_ENTRY;
        }

        Chunk_Node *last_chunk_node = &world->chunk_nodes[World::chunk_capacity - 1];
        last_chunk_node->next       = nullptr;

        for (u32 i = 0; i < World::chunk_capacity - 1; i++)
        {
            Chunk_Node *chunk_node = world->chunk_nodes + i;
            chunk_node->next = world->chunk_nodes + i + 1;
        }

        world->free_chunk_count      = World::chunk_capacity;
        world->first_free_chunk_node = &world->chunk_nodes[0];

        world->update_chunk_jobs_queue.initialize(world->update_chunk_jobs_queue_data, DEFAULT_QUEUE_SIZE);
        world->calculate_chunk_lighting_queue.initialize(world->calculate_chunk_lighting_queue_data, DEFAULT_QUEUE_SIZE);
        world->light_propagation_queue.initialize(world->light_propagation_queue_data, DEFAULT_QUEUE_SIZE);

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
        Assert(chunk_node >= world->chunk_nodes && chunk_node <= world->chunk_nodes + World::chunk_capacity);
        world->free_chunk_count++;
        chunk_node->next             = world->first_free_chunk_node;
        world->first_free_chunk_node = chunk_node;
    }

    bool insert_chunk(World *world, Chunk *chunk)
    {
        u16 chunk_hash = (u16)get_chunk_hash(chunk->world_coords);
        i16 count      = World::chunk_hash_table_capacity;
        while (world->chunk_hash_table[chunk_hash].chunk_node_index != INVALID_CHUNK_ENTRY && count--)
        {
            if (world->chunk_hash_table[chunk_hash].chunk_coords == chunk->world_coords)
            {
                return false;
            }
            chunk_hash++;
            if (chunk_hash == World::chunk_hash_table_capacity) chunk_hash = 0;
        }

        if (count > 0)
        {
            i16 chunk_index = (i16)((Chunk_Node*)chunk - world->chunk_nodes);
            world->chunk_hash_table[chunk_hash].chunk_coords     = chunk->world_coords;
            world->chunk_hash_table[chunk_hash].chunk_node_index = chunk_index;
            return true;
        }

        return false;
    }

    bool remove_chunk(World *world, glm::ivec2 coords)
    {
        u16 chunk_hash       = (u16)get_chunk_hash(coords);
        u16 chunk_hast_start = chunk_hash;
        do
        {
            if (world->chunk_hash_table[chunk_hash].chunk_node_index != INVALID_CHUNK_ENTRY &&
                world->chunk_hash_table[chunk_hash].chunk_coords == coords)
            {
                world->chunk_hash_table[chunk_hash].chunk_node_index = INVALID_CHUNK_ENTRY;
                return true;
            }
            chunk_hash++;
            if (chunk_hash == World::chunk_hash_table_capacity) chunk_hash = 0;
        }
        while (chunk_hash != chunk_hast_start);

        return false;
    }

    Chunk* get_chunk(World *world, glm::ivec2 coords)
    {
        u16 chunk_hash       = (u16)get_chunk_hash(coords);
        u16 chunk_hast_start = chunk_hash;
        do
        {
            Chunk_Hash_Table_Entry *entry = &world->chunk_hash_table[chunk_hash];
            if (entry->chunk_node_index != INVALID_CHUNK_ENTRY &&
                entry->chunk_coords == coords)
            {
                return &world->chunk_nodes[entry->chunk_node_index].chunk;
            }
            chunk_hash++;
            if (chunk_hash == World::chunk_hash_table_capacity) chunk_hash = 0;
        }
        while (chunk_hash != chunk_hast_start);

        return nullptr;
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

     bool is_chunk_in_region_bounds(const glm::ivec2& chunk_coords,
                                    const World_Region_Bounds& region_bounds)
     {
        return chunk_coords.x >= region_bounds.min.x && chunk_coords.x <= region_bounds.max.x &&
               chunk_coords.y >= region_bounds.min.y && chunk_coords.y <= region_bounds.max.y;
     }

    void load_chunks_at_region(World *world, const World_Region_Bounds& region_bounds)
    {
        for (i32 z = region_bounds.min.y - 1; z <= region_bounds.max.y + 1; ++z)
        {
            for (i32 x = region_bounds.min.x - 1; x <= region_bounds.max.x + 1; ++x)
            {
                glm::ivec2 chunk_coords = { x, z };
                Chunk* chunk = get_chunk(world, chunk_coords);
                if (!chunk)
                {
                    chunk = allocate_chunk(world);
                    initialize_chunk(chunk, chunk_coords);
                    insert_chunk(world, chunk);
                    Load_Chunk_Job load_chunk_job = {};
                    load_chunk_job.world = world;
                    load_chunk_job.chunk = chunk;
                    Job_System::schedule(load_chunk_job);
                }
            }
        }

        auto dirs = get_chunk_neighbour_directions();

        for (u32 i = 0; i < World::chunk_hash_table_capacity; i++)
        {
            if (world->chunk_hash_table[i].chunk_node_index == INVALID_CHUNK_ENTRY)
            {
                continue;
            }

            const glm::ivec2& chunk_coords = world->chunk_hash_table[i].chunk_coords;

            if (!is_chunk_in_region_bounds(chunk_coords, region_bounds))
            {
                continue;
            }

            Chunk *chunk = &world->chunk_nodes[world->chunk_hash_table[i].chunk_node_index].chunk;
            Assert(chunk);

            bool all_neighbours_loaded = true;

            for (i32 i = 0; i < ChunkNeighbour_Count; i++)
            {
                glm::ivec2 neighbour_chunk_coords = chunk_coords + dirs[i];

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
        }
    }

    void free_chunks_out_of_region(World *world, const World_Region_Bounds& region_bounds)
    {
        for (u32 i = 0; i < World::chunk_hash_table_capacity; i++)
        {
            if (world->chunk_hash_table[i].chunk_node_index == INVALID_CHUNK_ENTRY)
            {
                continue;
            }

            const glm::ivec2 &chunk_coords = world->chunk_hash_table[i].chunk_coords;
            Chunk *chunk = &world->chunk_nodes[world->chunk_hash_table[i].chunk_node_index].chunk;
            bool out_of_bounds = chunk_coords.x < region_bounds.min.x - World::pending_free_chunk_radius ||
                                 chunk_coords.x > region_bounds.max.x + World::pending_free_chunk_radius ||
                                 chunk_coords.y < region_bounds.min.y - World::pending_free_chunk_radius ||
                                 chunk_coords.y > region_bounds.max.y + World::pending_free_chunk_radius;

            if (out_of_bounds &&
                !chunk->pending_for_tessellation &&
                (chunk->state == ChunkState_Loaded ||
                 chunk->state == ChunkState_NeighboursLoaded ||
                 chunk->state == ChunkState_LightCalculated))
            {
                chunk->state = ChunkState_PendingForSave;

                Serialize_And_Free_Chunk_Job serialize_and_free_chunk_job;
                serialize_and_free_chunk_job.world = world;
                serialize_and_free_chunk_job.chunk = chunk;
                Job_System::schedule(serialize_and_free_chunk_job, false);

                remove_chunk(world, chunk->world_coords);
            }
        }

        for (u32 i = 0; i < World::chunk_capacity; i++)
        {
            Chunk* chunk = &world->chunk_nodes[i].chunk;
            if (chunk->state == ChunkState_Saved)
            {
                chunk->state = ChunkState_Freed;
                free_chunk(world, chunk);
            }
        }
    }

    static void queue_update_sub_chunk_job(Circular_FIFO_Queue< Update_Chunk_Job > &queue, Chunk *chunk, i32 sub_chunk_index)
    {
        Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];

        if (!render_data.pending_for_update)
        {
            render_data.pending_for_update = true;

            if (!chunk->pending_for_tessellation)
            {
                chunk->pending_for_tessellation = true;

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

        i32 sub_chunk_index = get_sub_chunk_index(block_coords);

        if (block_coords.x == 0)
        {
            Chunk *left_chunk = chunk->neighbours[ChunkNeighbour_Left];
            Assert(left_chunk);
            left_chunk->right_edge_blocks[block_coords.y * CHUNK_DEPTH + block_coords.z].id = block_id;
        }
        else if (block_coords.x == CHUNK_WIDTH - 1)
        {
            Chunk *right_chunk = chunk->neighbours[ChunkNeighbour_Right];
            Assert(right_chunk);
            right_chunk->left_edge_blocks[block_coords.y * CHUNK_DEPTH + block_coords.z].id = block_id;
        }

        if (block_coords.z == 0)
        {
            Chunk *front_chunk = chunk->neighbours[ChunkNeighbour_Front];
            Assert(front_chunk);
            front_chunk->back_edge_blocks[block_coords.y * CHUNK_WIDTH + block_coords.x].id = block_id;
        }
        else if (block_coords.z == CHUNK_DEPTH - 1)
        {
            Chunk *back_chunk = chunk->neighbours[ChunkNeighbour_Back];
            Assert(back_chunk);
            back_chunk->front_edge_blocks[block_coords.y * CHUNK_WIDTH + block_coords.x].id = block_id;
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
            Assert(left_chunk);
            left_chunk->right_edge_light_map[block_coords.y * CHUNK_DEPTH + block_coords.z].sky_light_level = light_level;
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, left_chunk, sub_chunk_index);
        }
        else if (block_coords.x == CHUNK_WIDTH - 1)
        {
            Chunk *right_chunk = chunk->neighbours[ChunkNeighbour_Right];
            Assert(right_chunk);
            right_chunk->left_edge_light_map[block_coords.y * CHUNK_DEPTH + block_coords.z].sky_light_level = light_level;
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, right_chunk, sub_chunk_index);
        }

        if (block_coords.z == 0)
        {
            Chunk *front_chunk = chunk->neighbours[ChunkNeighbour_Front];
            Assert(front_chunk);
            front_chunk->back_edge_light_map[block_coords.y * CHUNK_WIDTH + block_coords.x].sky_light_level = light_level;
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, front_chunk, sub_chunk_index);
        }
        else if (block_coords.z == CHUNK_DEPTH - 1)
        {
            Chunk *back_chunk = chunk->neighbours[ChunkNeighbour_Back];
            Assert(back_chunk);
            back_chunk->front_edge_light_map[block_coords.y * CHUNK_WIDTH + block_coords.x].sky_light_level = light_level;
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
            Assert(left_chunk);
            left_chunk->right_edge_light_map[block_coords.y * CHUNK_DEPTH + block_coords.z].light_source_level = light_level;
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, left_chunk, sub_chunk_index);
        }
        else if (block_coords.x == CHUNK_WIDTH - 1)
        {
            Chunk *right_chunk = chunk->neighbours[ChunkNeighbour_Right];
            Assert(right_chunk);
            right_chunk->left_edge_light_map[block_coords.y * CHUNK_DEPTH + block_coords.z].light_source_level = light_level;
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, right_chunk, sub_chunk_index);
        }

        if (block_coords.z == 0)
        {
            Chunk *front_chunk = chunk->neighbours[ChunkNeighbour_Front];
            Assert(front_chunk);
            front_chunk->back_edge_light_map[block_coords.y * CHUNK_WIDTH + block_coords.x].light_source_level = light_level;
            queue_update_sub_chunk_job(world->update_chunk_jobs_queue, front_chunk, sub_chunk_index);
        }
        else if (block_coords.z == CHUNK_DEPTH - 1)
        {
            Chunk *back_chunk = chunk->neighbours[ChunkNeighbour_Back];
            Assert(back_chunk);
            back_chunk->front_edge_light_map[block_coords.y * CHUNK_WIDTH + block_coords.x].light_source_level = light_level;
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
            if (block_coords.x >= 0 && block_coords.x < CHUNK_WIDTH &&
                block_coords.y >= 0 && block_coords.y < CHUNK_HEIGHT &&
                block_coords.z >= 0 && block_coords.z < CHUNK_WIDTH)
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
            result.block_coords = { CHUNK_WIDTH - 1, block_coords.y, block_coords.z };
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

        if (block_coords.x == CHUNK_WIDTH - 1)
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
            result.block_coords = { block_coords.x, block_coords.y, CHUNK_DEPTH - 1 };
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
        if (block_coords.z == CHUNK_DEPTH - 1)
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

    void schedule_chunk_lighting_jobs(World *world, const World_Region_Bounds &player_region_bounds)
    {
        auto& light_propagation_queue        = world->light_propagation_queue;
        auto& calculate_chunk_lighting_queue = world->calculate_chunk_lighting_queue;

        auto dirs = get_chunk_neighbour_directions();

        for (u32 i = 0; i < World::chunk_hash_table_capacity; i++)
        {
            if (world->chunk_hash_table[i].chunk_node_index == INVALID_CHUNK_ENTRY)
            {
                continue;
            }

            const glm::ivec2& chunk_coords = world->chunk_hash_table[i].chunk_coords;
            if (!is_chunk_in_region_bounds(chunk_coords, player_region_bounds))
            {
                continue;
            }

            Chunk *chunk = &world->chunk_nodes[world->chunk_hash_table[i].chunk_node_index].chunk;
            Assert(chunk);

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
    }

    void schedule_save_chunks_jobs(World *world)
    {
        for (u32 i = 0; i < World::chunk_hash_table_capacity; i++)
        {
            if (world->chunk_hash_table[i].chunk_node_index == INVALID_CHUNK_ENTRY)
            {
                continue;
            }

            Chunk *chunk = &world->chunk_nodes[world->chunk_hash_table[i].chunk_node_index].chunk;

            chunk->state = ChunkState_PendingForSave;

            Serialize_Chunk_Job job;
            job.world = world;
            job.chunk = chunk;
            Job_System::schedule(job);
        }
    }

    String8 get_chunk_file_path(World *world,
                                Chunk *chunk,
                                Temprary_Memory_Arena *temp_arena)
    {
        String8 chunk_file_path = push_formatted_string8_null_terminated(temp_arena,
                                                                         "%.*s/chunk_%d_%d.pkg",
                                                                         (i32)world->path.count,
                                                                         world->path.data,
                                                                         chunk->world_coords.x,
                                                                         chunk->world_coords.y);
        return chunk_file_path;
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