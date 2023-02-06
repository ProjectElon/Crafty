#include "chunk.h"

#include "memory/memory_arena.h"
#include "core/file_system.h"
#include "game/jobs.h"
#include "game/world.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/noise.hpp>
#include <glm/gtx/compatibility.hpp>

namespace minecraft {

    bool initialize_sub_chunk_bucket(Sub_Chunk_Bucket *sub_chunk_bucket)
    {
        sub_chunk_bucket->memory_id      = -1;
        sub_chunk_bucket->current_vertex = nullptr;
        sub_chunk_bucket->face_count     = 0;
        return true;
    }

    bool is_sub_chunk_bucket_allocated(const Sub_Chunk_Bucket *sub_chunk_bucket)
    {
        return sub_chunk_bucket->memory_id != -1 &&
               sub_chunk_bucket->current_vertex != nullptr;
    }

    i32 get_block_index(const glm::ivec3& block_coords)
    {
        Assert(block_coords.x >= 0 && block_coords.x < Chunk::Width &&
               block_coords.y >= 0 && block_coords.y < Chunk::Height &&
               block_coords.z >= 0 && block_coords.z < Chunk::Depth);

        return block_coords.y * Chunk::Width * Chunk::Depth + block_coords.z * Chunk::Width + block_coords.x;
    }

    glm::vec3 get_block_position(Chunk *chunk, const glm::ivec3& block_coords)
    {
        return chunk->position + glm::vec3((f32)block_coords.x + 0.5f, (f32)block_coords.y + 0.5f, (f32)block_coords.z + 0.5f);
    }

    Block* get_block(Chunk *chunk, const glm::ivec3& block_coords)
    {
        // Assert(chunk);
        i32 block_index = get_block_index(block_coords);
        return chunk->blocks + block_index;
    }

    Block_Light_Info* get_block_light_info(Chunk *chunk, const glm::ivec3& block_coords)
    {
        // Assert(chunk);
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
        chunk->position = { world_coords.x * Chunk::Width, 0.0f, world_coords.y * Chunk::Depth };

        for (i32 sub_chunk_index = 0; sub_chunk_index < Chunk::SubChunkCount; ++sub_chunk_index)
        {
            Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];

            render_data.face_count         =  0;
            render_data.bucket_index       =  0;
            render_data.instance_memory_id = -1;

            for (i32 j = 0; j < 2; j++)
            {
                initialize_sub_chunk_bucket(&render_data.opaque_buckets[j]);
                initialize_sub_chunk_bucket(&render_data.transparent_buckets[j]);

                constexpr f32 inf = std::numeric_limits< f32 >::max();
                render_data.aabb[j] = { { inf, inf, inf }, { -inf, -inf, -inf } };
            }

            render_data.state = TessellationState_None;
        }

        chunk->state              = ChunkState_Initialized;
        chunk->tessellation_state = TessellationState_None;

        for (i32 i = 0; i < ChunkNeighbour_Count; i++)
        {
            chunk->neighbours[i] = nullptr;
        }

        return true;
    }

    inline static glm::vec2 get_sample(i32 seed, const glm::ivec2& chunk_coords, const glm::ivec2& block_xz_coords)
    {
        return {
            seed + chunk_coords.x * Chunk::Width + block_xz_coords.x + 0.5f,
            seed + chunk_coords.y * Chunk::Depth + block_xz_coords.y + 0.5f };
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
        i32 height_map[Chunk::Depth][Chunk::Width];

        i32 top_edge_height_map[Chunk::Width];
        i32 bottom_edge_height_map[Chunk::Width];
        i32 left_edge_height_map[Chunk::Depth];
        i32 right_edge_height_map[Chunk::Depth];

        constexpr i32 min_biome_height = 100;
        constexpr i32 max_biome_height = 250;
        constexpr i32 water_level = min_biome_height + 50;
        static_assert(water_level >= min_biome_height && water_level <= max_biome_height);

        const glm::ivec2 front_chunk_coords = { chunk->world_coords.x + 0, chunk->world_coords.y - 1 };
        const glm::ivec2 back_chunk_coords  = { chunk->world_coords.x + 0, chunk->world_coords.y + 1 };
        const glm::ivec2 left_chunk_coords  = { chunk->world_coords.x - 1, chunk->world_coords.y + 0 };
        const glm::ivec2 right_chunk_coords = { chunk->world_coords.x + 1, chunk->world_coords.y + 0 };

        for (i32 z = 0; z < Chunk::Depth; ++z)
        {
            for (i32 x = 0; x < Chunk::Width; ++x)
            {
                glm::ivec2 block_xz_coords = { x, z };
                glm::vec2 sample = get_sample(seed, chunk->world_coords, block_xz_coords);
                f32 noise = get_noise01(sample);
                Assert(noise >= 0.0f && noise <= 1.0f);
                height_map[z][x] = get_height_from_noise01(min_biome_height, max_biome_height, noise);
            }
        }

        for (i32 x = 0; x < Chunk::Width; ++x)
        {
            glm::ivec2 block_xz_coords = { x, Chunk::Depth - 1 };
            glm::vec2 sample = get_sample(seed, front_chunk_coords, block_xz_coords);
            f32 noise = get_noise01(sample);
            top_edge_height_map[x] = get_height_from_noise01(min_biome_height, max_biome_height, noise);
        }

        for (i32 x = 0; x < Chunk::Width; ++x)
        {
            glm::ivec2 block_xz_coords = { x, 0 };
            glm::vec2 sample = get_sample(seed, back_chunk_coords, block_xz_coords);
            f32 noise = get_noise01(sample);
            bottom_edge_height_map[x] = get_height_from_noise01(min_biome_height, max_biome_height, noise);
        }

        for (i32 z = 0; z < Chunk::Depth; ++z)
        {
            glm::ivec2 block_xz_coords = { Chunk::Width - 1, z };
            glm::vec2 sample = get_sample(seed, left_chunk_coords, block_xz_coords);
            f32 noise = get_noise01(sample);
            left_edge_height_map[z] = get_height_from_noise01(min_biome_height, max_biome_height, noise);
        }

        for (i32 z = 0; z < Chunk::Depth; ++z)
        {
            glm::ivec2 block_xz_coords = { 0, z };
            glm::vec2 sample = get_sample(seed, right_chunk_coords, block_xz_coords);
            f32 noise = get_noise01(sample);
            right_edge_height_map[z] = get_height_from_noise01(min_biome_height, max_biome_height, noise);
        }

        for (i32 y = 0; y < Chunk::Height; ++y)
        {
            for (i32 z = 0; z < Chunk::Depth; ++z)
            {
                for (i32 x = 0; x < Chunk::Width; ++x)
                {
                    glm::ivec3 block_coords = { x, y, z };
                    Block *block = get_block(chunk, block_coords);
                    const i32& height = height_map[z][x];
                    set_block_id_based_on_height(block, y, height, min_biome_height, max_biome_height, water_level);
                }
            }
        }

        for (i32 y = 0; y < Chunk::Height; ++y)
        {
            for (i32 x = 0; x < Chunk::Width; ++x)
            {
                const i32& top_edge_height = top_edge_height_map[x];
                Block *top_edge_block = &(chunk->front_edge_blocks[y * Chunk::Width + x]);
                set_block_id_based_on_height(top_edge_block, y, top_edge_height, min_biome_height, max_biome_height, water_level);

                const i32& bottom_edge_height = bottom_edge_height_map[x];
                Block *bottom_edge_block = &(chunk->back_edge_blocks[y * Chunk::Width + x]);
                set_block_id_based_on_height(bottom_edge_block, y, bottom_edge_height, min_biome_height, max_biome_height, water_level);
            }

            for (i32 z = 0; z < Chunk::Depth; ++z)
            {
                const i32& left_edge_height = left_edge_height_map[z];
                Block *left_edge_block = &(chunk->left_edge_blocks[y * Chunk::Depth + z]);
                set_block_id_based_on_height(left_edge_block, y, left_edge_height, min_biome_height, max_biome_height, water_level);

                const i32& right_edge_height = right_edge_height_map[z];
                Block *right_edge_block = &(chunk->right_edge_blocks[y * Chunk::Depth + z]);
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
        Block_Serialization_Info serialized_blocks[Chunk::Height * Chunk::Depth * Chunk::Width];

        for (i32 block_index = 0; block_index < Chunk::Height * Chunk::Depth * Chunk::Width; block_index++)
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
        Block_Serialization_Info serialized_front_edge_blocks[Chunk::Height * Chunk::Width];

        for (u32 block_index = 0; block_index < Chunk::Height * Chunk::Width; block_index++)
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
        Block_Serialization_Info serialized_back_edge_blocks[Chunk::Height  * Chunk::Width];

        for (u32 block_index = 0; block_index < Chunk::Height * Chunk::Width; block_index++)
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
        Block_Serialization_Info serialized_left_edge_blocks[Chunk::Height * Chunk::Depth];

        for (u32 block_index = 0; block_index < Chunk::Height * Chunk::Width; block_index++)
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
        Block_Serialization_Info serialized_right_edge_blocks[Chunk::Height * Chunk::Depth];

        for (u32 block_index = 0; block_index < Chunk::Height * Chunk::Width; block_index++)
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
            Block_Serialization_Info serialized_blocks[Chunk::Height * Chunk::Depth * Chunk::Width];
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
            Block_Serialization_Info serialized_front_edge_blocks[Chunk::Height * Chunk::Width];
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
            Block_Serialization_Info serialized_back_edge_blocks[Chunk::Height * Chunk::Width];
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
            Block_Serialization_Info serialized_left_edge_blocks[Chunk::Height * Chunk::Depth];
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
            Block_Serialization_Info serialized_right_edge_blocks[Chunk::Height * Chunk::Depth];
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

    void propagate_sky_light(World *world, Chunk *chunk, Circular_Queue< Block_Query_Result > *queue)
    {
        for (i32 z = 0; z < Chunk::Depth; z++)
        {
            for (i32 x = 0; x < Chunk::Width; x++)
            {
                bool can_sky_light_propagate = true;

                for (i32 y = Chunk::Height - 1; y >= 0; y--)
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
                            Circular_Queue< Block_Query_Result > *queue)
    {
        for (i32 y = Chunk::Height - 1; y >= 0; y--)
        {
            bool found_any_sky_lights = false;
            for (i32 z = 0; z < Chunk::Width; z++)
            {
                for (i32 x = 0; x < Chunk::Width; x++)
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
                            Block_Light_Info *neighbour_light_info = get_block_light_info(neighbour_query.chunk,
                                                                                          neighbour_query.block_coords);
                            if (neighbour_light_info->sky_light_level != 15 &&
                                is_block_transparent(neighbour_info))
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

    Block* get_neighbour_block_from_right(Chunk *chunk, const glm::ivec3& block_coords)
    {
        if (block_coords.x == Chunk::Width - 1)
        {
            return &(chunk->right_edge_blocks[block_coords.y * Chunk::Depth + block_coords.z]);
        }

        return get_block(chunk, { block_coords.x + 1, block_coords.y, block_coords.z });
    }

    Block* get_neighbour_block_from_left(Chunk *chunk, const glm::ivec3& block_coords)
    {
        if (block_coords.x == 0)
        {
            return &(chunk->left_edge_blocks[block_coords.y * Chunk::Depth + block_coords.z]);
        }

        return get_block(chunk, { block_coords.x - 1, block_coords.y, block_coords.z });
    }

    Block* get_neighbour_block_from_top(Chunk *chunk, const glm::ivec3& block_coords)
    {
        if (block_coords.y == Chunk::Height - 1)
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
            return &(chunk->front_edge_blocks[block_coords.y * Chunk::Width + block_coords.x]);
        }
        return get_block(chunk, { block_coords.x, block_coords.y, block_coords.z - 1 });
    }

    Block* get_neighbour_block_from_back(Chunk *chunk, const glm::ivec3& block_coords)
    {
        if (block_coords.z == Chunk::Depth - 1)
        {
            return &(chunk->back_edge_blocks[block_coords.y * Chunk::Width + block_coords.x]);
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

    String8 get_chunk_file_path(World *world,
                                Chunk *chunk,
                                Temprary_Memory_Arena *temp_arena)
    {
        String8 chunk_file_path = push_string8(temp_arena,
                                               "%.*s/chunk_%d_%d.pkg",
                                               (i32)world->path.count,
                                               world->path.data,
                                               chunk->world_coords.x,
                                               chunk->world_coords.y);
        return chunk_file_path;
    }
}