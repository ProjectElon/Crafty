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

extern int errno;

namespace minecraft {

    bool Chunk::initialize(const glm::ivec2& world_coords)
    {
        this->world_coords = world_coords;
        this->position = { world_coords.x * MC_CHUNK_WIDTH, 0.0f, world_coords.y * MC_CHUNK_DEPTH };

        for (i32 sub_chunk_index = 0; sub_chunk_index < World::sub_chunk_count_per_chunk; ++sub_chunk_index)
        {
            Sub_Chunk_Render_Data& render_data = this->sub_chunks_render_data[sub_chunk_index];

            render_data.face_count      = 0;
            render_data.instance_memory_id = -1;

            render_data.opaque_bucket.memory_id = -1;
            render_data.opaque_bucket.current_vertex = nullptr;
            render_data.opaque_bucket.face_count = 0;

            render_data.transparent_bucket.memory_id = -1;
            render_data.transparent_bucket.current_vertex = nullptr;
            render_data.transparent_bucket.face_count = 0;

            render_data.uploaded_to_gpu = false;
            render_data.pending_for_update = false;
            constexpr f32 infinity = std::numeric_limits<f32>::max();
            render_data.aabb = { { infinity, infinity, infinity }, { -infinity, -infinity, -infinity } };
        }

        this->loaded  = false;
        this->pending_for_load = true;
        this->pending_for_save = false;

        std::stringstream ss;
        ss << World::path << "/chunk_" << this->world_coords.x << "_" << this->world_coords.y << ".pkg";
        this->file_path = ss.str();

        return true;
    }

    inline static glm::vec2 get_sample(i32 seed, const glm::ivec2& chunk_coords, const glm::ivec2& block_xz_coords)
    {
        return {
            seed + chunk_coords.x * MC_CHUNK_WIDTH + block_xz_coords.x + 0.5f,
            seed + chunk_coords.y * MC_CHUNK_DEPTH + block_xz_coords.y + 0.5f };
    }

    inline static f32 get_noise01(const glm::vec2& sample, f32 one_over_noise_scale)
    {
        f32 noise = glm::perlin(sample * one_over_noise_scale);
        return (noise + 1.0f) / 2.0f;
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

    void Chunk::generate(i32 seed)
    {
        i32 height_map[MC_CHUNK_DEPTH][MC_CHUNK_WIDTH];

        i32 top_edge_height_map[MC_CHUNK_WIDTH];
        i32 bottom_edge_height_map[MC_CHUNK_WIDTH];
        i32 left_edge_height_map[MC_CHUNK_DEPTH];
        i32 right_edge_height_map[MC_CHUNK_DEPTH];

        const i32 min_biome_height = 120;
        const i32 max_biome_height = 200;
        const i32 water_level = 140;
        assert(water_level >= min_biome_height);

        const f32 noise_scale = 69.0f; // a nice noise scale
        const f32 one_over_nosie_scale = 1.0f / noise_scale;

        const glm::ivec2 front_chunk_coords = { world_coords.x + 0, world_coords.y - 1 };
        const glm::ivec2 back_chunk_coords  = { world_coords.x + 0, world_coords.y + 1 };
        const glm::ivec2 left_chunk_coords  = { world_coords.x - 1, world_coords.y + 0 };
        const glm::ivec2 right_chunk_coords = { world_coords.x + 1, world_coords.y + 0 };

        for (i32 z = 0; z < MC_CHUNK_DEPTH; ++z)
        {
            for (i32 x = 0; x < MC_CHUNK_WIDTH; ++x)
            {
                glm::ivec2 block_xz_coords = { x, z };
                glm::vec2 sample = get_sample(seed, world_coords, block_xz_coords);
                f32 noise = get_noise01(sample, one_over_nosie_scale);
                height_map[z][x] = get_height_from_noise01(min_biome_height, max_biome_height, noise);
            }
        }

        for (i32 x = 0; x < MC_CHUNK_WIDTH; ++x)
        {
            glm::ivec2 block_xz_coords = { x, MC_CHUNK_DEPTH - 1 };
            glm::vec2 sample = get_sample(seed, front_chunk_coords, block_xz_coords);
            f32 noise = get_noise01(sample, one_over_nosie_scale);
            top_edge_height_map[x] = get_height_from_noise01(min_biome_height, max_biome_height, noise);
        }

        for (i32 x = 0; x < MC_CHUNK_WIDTH; ++x)
        {
            glm::ivec2 block_xz_coords = { x, 0 };
            glm::vec2 sample = get_sample(seed, back_chunk_coords, block_xz_coords);
            f32 noise = get_noise01(sample, one_over_nosie_scale);
            bottom_edge_height_map[x] = get_height_from_noise01(min_biome_height, max_biome_height, noise);
        }

        for (i32 z = 0; z < MC_CHUNK_DEPTH; ++z)
        {
            glm::ivec2 block_xz_coords = { MC_CHUNK_WIDTH - 1, z };
            glm::vec2 sample = get_sample(seed, left_chunk_coords, block_xz_coords);
            f32 noise = get_noise01(sample, one_over_nosie_scale);
            left_edge_height_map[z] = get_height_from_noise01(min_biome_height, max_biome_height, noise);
        }

        for (i32 z = 0; z < MC_CHUNK_DEPTH; ++z)
        {
            glm::ivec2 block_xz_coords = { 0, z };
            glm::vec2 sample = get_sample(seed, right_chunk_coords, block_xz_coords);
            f32 noise = get_noise01(sample, one_over_nosie_scale);
            right_edge_height_map[z] = get_height_from_noise01(min_biome_height, max_biome_height, noise);
        }

        for (i32 y = 0; y < MC_CHUNK_HEIGHT; ++y)
        {
            for (i32 z = 0; z < MC_CHUNK_DEPTH; ++z)
            {
                for (i32 x = 0; x < MC_CHUNK_WIDTH; ++x)
                {
                    glm::ivec3 block_coords = { x, y, z };
                    Block *block = this->get_block(block_coords);
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
                Block *top_edge_block = &(front_edge_blocks[y * MC_CHUNK_WIDTH + x]);
                set_block_id_based_on_height(top_edge_block, y, top_edge_height, min_biome_height, max_biome_height, water_level);

                const i32& bottom_edge_height = bottom_edge_height_map[x];
                Block *bottom_edge_block = &(back_edge_blocks[y * MC_CHUNK_WIDTH + x]);
                set_block_id_based_on_height(bottom_edge_block, y, bottom_edge_height, min_biome_height, max_biome_height, water_level);
            }

            for (i32 z = 0; z < MC_CHUNK_DEPTH; ++z)
            {
                const i32& left_edge_height = left_edge_height_map[z];
                Block *left_edge_block = &(left_edge_blocks[y * MC_CHUNK_DEPTH + z]);
                set_block_id_based_on_height(left_edge_block, y, left_edge_height, min_biome_height, max_biome_height, water_level);

                const i32& right_edge_height = right_edge_height_map[z];
                Block *right_edge_block = &(right_edge_blocks[y * MC_CHUNK_DEPTH + z]);
                set_block_id_based_on_height(right_edge_block, y, right_edge_height, min_biome_height, max_biome_height, water_level);
            }
        }

        this->loaded = true;
    }

    void Chunk::serialize()
    {
        FILE *file = fopen(file_path.c_str(), "wb");
        if (file == NULL)
        {
            fprintf(stderr, "[ERROR]: failed to open file %s for writing: %s\n", file_path.c_str(), strerror(errno));
            return;
        }
        assert(this->loaded);
        fwrite(blocks, sizeof(Block) * MC_CHUNK_WIDTH * MC_CHUNK_DEPTH * MC_CHUNK_HEIGHT, 1, file);
        fwrite(front_edge_blocks, sizeof(Block) * MC_CHUNK_WIDTH * MC_CHUNK_HEIGHT, 1, file);
        fwrite(back_edge_blocks,  sizeof(Block) * MC_CHUNK_WIDTH * MC_CHUNK_HEIGHT, 1, file);
        fwrite(left_edge_blocks,  sizeof(Block) * MC_CHUNK_DEPTH * MC_CHUNK_HEIGHT, 1, file);
        fwrite(right_edge_blocks, sizeof(Block) * MC_CHUNK_DEPTH * MC_CHUNK_HEIGHT, 1, file);
        fclose(file);
    }

    void Chunk::deserialize()
    {
        FILE *file = fopen(file_path.c_str(), "rb");
        if (file == NULL)
        {
            fprintf(stderr, "[ERROR]: failed to open file %s for reading: %s\n", file_path.c_str(), strerror(errno));
            return;
        }
        assert(!this->loaded);
        fread(blocks, sizeof(Block) * MC_CHUNK_WIDTH * MC_CHUNK_DEPTH * MC_CHUNK_HEIGHT, 1, file);
        fread(front_edge_blocks, sizeof(Block) * MC_CHUNK_DEPTH * MC_CHUNK_HEIGHT, 1, file);
        fread(back_edge_blocks,  sizeof(Block) * MC_CHUNK_DEPTH * MC_CHUNK_HEIGHT, 1, file);
        fread(left_edge_blocks,  sizeof(Block) * MC_CHUNK_WIDTH * MC_CHUNK_HEIGHT, 1, file);
        fread(right_edge_blocks, sizeof(Block) * MC_CHUNK_WIDTH * MC_CHUNK_HEIGHT, 1, file);
        fclose(file);
        this->loaded = true;
    }

    Block* Chunk::get_block(const glm::ivec3& block_coords)
    {
        assert(block_coords.x >= 0 && block_coords.x < MC_CHUNK_WIDTH &&
               block_coords.y >= 0 && block_coords.y < MC_CHUNK_HEIGHT &&
               block_coords.z >= 0 && block_coords.z < MC_CHUNK_DEPTH);
        i32 block_index = get_block_index(block_coords);
        return this->blocks + block_index;
    }

    Block* Chunk::get_neighbour_block_from_right(const glm::ivec3& block_coords)
    {
        if (block_coords.x == MC_CHUNK_WIDTH - 1)
        {
            return &(right_edge_blocks[block_coords.y * MC_CHUNK_DEPTH + block_coords.z]);
        }

        return this->get_block({ block_coords.x + 1, block_coords.y, block_coords.z });
    }

    Block* Chunk::get_neighbour_block_from_left(const glm::ivec3& block_coords)
    {
        if (block_coords.x == 0)
        {
            return &(left_edge_blocks[block_coords.y * MC_CHUNK_DEPTH + block_coords.z]);
        }

        return this->get_block({ block_coords.x - 1, block_coords.y, block_coords.z });
    }

    Block* Chunk::get_neighbour_block_from_top(const glm::ivec3& block_coords)
    {
        if (block_coords.y == MC_CHUNK_HEIGHT - 1)
        {
            return &World::null_block;
        }

        return this->get_block({ block_coords.x, block_coords.y + 1, block_coords.z });
    }

    Block* Chunk::get_neighbour_block_from_bottom(const glm::ivec3& block_coords)
    {
        if (block_coords.y == 0)
        {
            return &World::null_block;
        }
        return this->get_block({ block_coords.x, block_coords.y - 1, block_coords.z });
    }

    Block* Chunk::get_neighbour_block_from_front(const glm::ivec3& block_coords)
    {
        if (block_coords.z == 0)
        {
            return &(front_edge_blocks[block_coords.y * MC_CHUNK_WIDTH + block_coords.x]);
        }
        return this->get_block({ block_coords.x, block_coords.y, block_coords.z - 1 });
    }

    Block* Chunk::get_neighbour_block_from_back(const glm::ivec3& block_coords)
    {
        if (block_coords.z == MC_CHUNK_DEPTH - 1)
        {
            return &(back_edge_blocks[block_coords.y * MC_CHUNK_WIDTH + block_coords.x]);
        }

        return this->get_block({ block_coords.x, block_coords.y, block_coords.z + 1 });
    }

    bool World::initialize(const std::string& world_path)
    {
        namespace fs = std::filesystem;

        if (!fs::exists(fs::path(world_path)))
        {
            fs::create_directories(fs::path(world_path));
        }

        World::path = world_path;
        std::string meta_file_path = world_path + "/meta";
        FILE* meta_file = fopen(meta_file_path.c_str(), "rb");

        if (meta_file)
        {
            fscanf(meta_file, "%d", &World::seed);
        }
        else
        {
            srand((u32)time(nullptr));
            World::seed = (i32)(((f32)rand() / (f32)RAND_MAX) * 1000000.0f);
            meta_file = fopen(meta_file_path.c_str(), "wb");
            fprintf(meta_file, "%d", World::seed);
        }

        fclose(meta_file);

        // todo(harlequin): do more testing on the std::unordered_map or replace it at some point
        loaded_chunks.reserve(2 * World::chunk_capacity);
        loaded_chunks.max_load_factor(0.25);
        update_sub_chunk_jobs.reserve(1024);
        chunk_pool.initialize();

        return true;
    }

    Block_Query_Result World::select_block(const glm::vec3& view_position,
                                           const glm::vec3& view_direction,
                                           u32 max_block_select_dist_in_cube_units,
                                           Ray_Cast_Result *out_ray_cast_result)
    {
        Block_Query_Result result;
        result.chunk = nullptr;
        result.block = nullptr;
        result.block_coords = { -1, -1, -1 };

        glm::vec3 query_position = view_position;

        for (u32 i = 0; i < max_block_select_dist_in_cube_units * 10; i++)
        {
            Block_Query_Result query = World::query_block(query_position);

            if (query.chunk && query.block && query.block->id != BlockId_Air && query.block->id != BlockId_Water)
            {
                glm::vec3 block_position = query.chunk->get_block_position(query.block_coords);
                Ray ray   = { view_position, view_direction };
                AABB aabb = { block_position - glm::vec3(0.5f, 0.5f, 0.5f), block_position + glm::vec3(0.5f, 0.5f, 0.5f) };
                Ray_Cast_Result ray_cast_result = cast_ray_on_aabb(ray, aabb);

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

    void World::load_chunks_at_region(const World_Region_Bounds& region_bounds)
    {
        for (i32 z = region_bounds.min.y; z <= region_bounds.max.y; ++z)
        {
            for (i32 x = region_bounds.min.x; x <= region_bounds.max.x; ++x)
            {
                glm::ivec2 chunk_coords = { x, z };
                auto it = loaded_chunks.find(chunk_coords);

                if (it == loaded_chunks.end())
                {
                    chunk_pool_mutex.lock();
                    Chunk* chunk = chunk_pool.allocate();
                    chunk_pool_mutex.unlock();

                    chunk->initialize(chunk_coords);
                    loaded_chunks.emplace(chunk_coords, chunk);

                    Load_Chunk_Job load_chunk_job;
                    load_chunk_job.chunk = chunk;
                    Job_System::schedule(load_chunk_job);
                }
            }
        }
    }

    void World::update_sub_chunks()
    {
        for (const auto& job : update_sub_chunk_jobs)
        {
            Job_System::schedule(job);
        }

        update_sub_chunk_jobs.resize(0);
    }

    void World::free_chunks_out_of_region(const World_Region_Bounds& region_bounds)
    {
        for (auto it = loaded_chunks.begin(); it != loaded_chunks.end();)
        {
            auto chunk_coords = it->first;
            Chunk* chunk = it->second;

            bool out_of_bounds = chunk_coords.x < region_bounds.min.x - 1 ||
                                 chunk_coords.x > region_bounds.max.x + 1 ||
                                 chunk_coords.y < region_bounds.min.y - 1 ||
                                 chunk_coords.y > region_bounds.max.y + 1;

            if (!chunk->pending_for_load && !chunk->pending_for_save && chunk->loaded && out_of_bounds)
            {
                for (i32 sub_chunk_index = 0; sub_chunk_index < World::sub_chunk_count_per_chunk; ++sub_chunk_index)
                {
                    Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];

                    if (render_data.uploaded_to_gpu)
                    {
                        Opengl_Renderer::free_sub_chunk(chunk, sub_chunk_index);
                    }
                }

                chunk->pending_for_save = true;

                Serialize_And_Free_Chunk_Job serialize_and_free_chunk_job;
                serialize_and_free_chunk_job.chunk = chunk;
                Job_System::schedule(serialize_and_free_chunk_job);

                it = loaded_chunks.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    static void queue_update_sub_chunk_job(std::vector<Update_Sub_Chunk_Job> &jobs, Chunk *chunk, i32 sub_chunk_index)
    {
        Sub_Chunk_Render_Data& render_data = chunk->sub_chunks_render_data[sub_chunk_index];

        if (!render_data.pending_for_update)
        {
            render_data.pending_for_update = true;
            Update_Sub_Chunk_Job job;
            job.chunk = chunk;
            job.sub_chunk_index = sub_chunk_index;
            jobs.emplace_back(job);
        }
    }

    void World::set_block_id(Chunk *chunk, const glm::ivec3& block_coords, u16 block_id)
    {
        Block *block = chunk->get_block(block_coords);
        block->id = block_id;

        i32 sub_chunk_index = World::get_sub_chunk_index(block_coords);
        queue_update_sub_chunk_job(update_sub_chunk_jobs, chunk, sub_chunk_index);

        if (block_coords.x == 0)
        {
            Chunk *left_chunk = World::get_chunk({ chunk->world_coords.x - 1, chunk->world_coords.y });
            assert(left_chunk);
            left_chunk->right_edge_blocks[block_coords.y * MC_CHUNK_DEPTH + block_coords.z].id = block_id;
            queue_update_sub_chunk_job(update_sub_chunk_jobs, left_chunk, sub_chunk_index);
        }
        else if (block_coords.x == MC_CHUNK_WIDTH - 1)
        {
            Chunk *right_chunk = World::get_chunk({ chunk->world_coords.x + 1, chunk->world_coords.y });
            assert(right_chunk);
            right_chunk->left_edge_blocks[block_coords.y * MC_CHUNK_DEPTH + block_coords.z].id = block_id;
            queue_update_sub_chunk_job(update_sub_chunk_jobs, right_chunk, sub_chunk_index);
        }

        if (block_coords.z == 0)
        {
            Chunk *front_chunk = World::get_chunk({ chunk->world_coords.x, chunk->world_coords.y - 1 });
            assert(front_chunk);
            front_chunk->back_edge_blocks[block_coords.y * MC_CHUNK_WIDTH + block_coords.x].id = block_id;
            queue_update_sub_chunk_job(update_sub_chunk_jobs, front_chunk, sub_chunk_index);
        }
        else if (block_coords.z == MC_CHUNK_DEPTH - 1)
        {
            Chunk *back_chunk = World::get_chunk({ chunk->world_coords.x, chunk->world_coords.y + 1 });
            assert(back_chunk);
            back_chunk->front_edge_blocks[block_coords.y * MC_CHUNK_WIDTH + block_coords.x].id = block_id;
            queue_update_sub_chunk_job(update_sub_chunk_jobs, back_chunk, sub_chunk_index);
        }

        i32 sub_chunk_start_y = sub_chunk_index * World::sub_chunk_height;
        i32 sub_chunk_end_y = (sub_chunk_index + 1) * World::sub_chunk_height - 1;

        if (block_coords.y == sub_chunk_end_y && sub_chunk_index != World::sub_chunk_count_per_chunk - 1)
        {
            queue_update_sub_chunk_job(update_sub_chunk_jobs, chunk, sub_chunk_index + 1);
        }
        else if (block_coords.y == sub_chunk_start_y && sub_chunk_index != 0)
        {
            queue_update_sub_chunk_job(update_sub_chunk_jobs, chunk, sub_chunk_index - 1);
        }
    }

    Block_Query_Result World::get_neighbour_block_from_top(Chunk *chunk, const glm::ivec3& block_coords)
    {
        Block_Query_Result result;
        result.block_coords = { block_coords.x, block_coords.y + 1, block_coords.z };
        result.chunk = chunk;
        result.block = chunk->get_neighbour_block_from_top(block_coords);
        return result;
    }

    Block_Query_Result World::get_neighbour_block_from_bottom(Chunk *chunk, const glm::ivec3& block_coords)
    {
        Block_Query_Result result;
        result.block_coords = { block_coords.x, block_coords.y - 1, block_coords.z };
        result.chunk = chunk;
        result.block = chunk->get_neighbour_block_from_bottom(block_coords);
        return result;
    }

    Block_Query_Result World::get_neighbour_block_from_left(Chunk *chunk, const glm::ivec3& block_coords)
    {
        Block_Query_Result result;

        if (block_coords.x == 0)
        {
            result.chunk = World::get_chunk({ chunk->world_coords.x - 1, chunk->world_coords.y });
            result.block_coords = { MC_CHUNK_WIDTH - 1, block_coords.y, block_coords.z };
        }
        else
        {
            result.chunk = chunk;
            result.block_coords = { block_coords.x - 1, block_coords.y, block_coords.z };
        }

        result.block = result.chunk->get_block(result.block_coords);
        return result;
    }

    Block_Query_Result World::get_neighbour_block_from_right(Chunk *chunk, const glm::ivec3& block_coords)
    {
        Block_Query_Result result;

        if (block_coords.x == MC_CHUNK_WIDTH - 1)
        {
            result.chunk = World::get_chunk({ chunk->world_coords.x + 1, chunk->world_coords.y });
            result.block_coords = { 0, block_coords.y, block_coords.z };
        }
        else
        {
            result.chunk = chunk;
            result.block_coords = { block_coords.x + 1, block_coords.y, block_coords.z };
        }

        result.block = result.chunk->get_block(result.block_coords);
        return result;
    }

    Block_Query_Result World::get_neighbour_block_from_front(Chunk *chunk, const glm::ivec3& block_coords)
    {
        Block_Query_Result result;

        if (block_coords.z == 0)
        {
            result.chunk = World::get_chunk({ chunk->world_coords.x, chunk->world_coords.y - 1 });
            result.block_coords = { block_coords.x, block_coords.y, MC_CHUNK_DEPTH - 1 };
        }
        else
        {
            result.chunk = chunk;
            result.block_coords = { block_coords.x, block_coords.y, block_coords.z - 1 };
        }

        result.block = result.chunk->get_block(result.block_coords);
        return result;
    }

    Block_Query_Result World::get_neighbour_block_from_back(Chunk *chunk, const glm::ivec3& block_coords)
    {
        Block_Query_Result result;
        if (block_coords.z == MC_CHUNK_DEPTH - 1)
        {
            result.chunk = World::get_chunk({ chunk->world_coords.x, chunk->world_coords.y + 1 });
            result.block_coords = { block_coords.x, block_coords.y, 0 };
        }
        else
        {
            result.chunk = chunk;
            result.block_coords = { block_coords.x, block_coords.y, block_coords.z + 1 };
        }
        result.block = result.chunk->get_block(result.block_coords);
        return result;
    }

    void World::set_block_to_place_command(const std::vector<Command_Argument>& args)
    {
        if (args[0].type == CommandArgumentType_String)
        {
            std::string block_name = std::get<std::string>(args[0].data);
            i32 block_id = -1;
            for (i32 i = 0; i < BlockId_Count; i++)
            {
                const Block_Info& block_info = block_infos[i];
                if (block_name == block_info.name)
                {
                    block_id = i;
                    break;
                }
            }

            if (block_id != -1) {
                block_to_place_id = (u16)block_id;
            } else {
                Dropdown_Console::log("invalid block name");
            }
        }
    }

    void World::blocks_command(const std::vector<Command_Argument>& args)
    {
        for (i32 i = 0; i < BlockId_Count; i++)
        {
            const Block_Info& block_info = block_infos[i];
            Dropdown_Console::log(block_info.name);
        }
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
            "oak_glow_stone",
            Texture_Id_glowstone,
            Texture_Id_glowstone,
            Texture_Id_glowstone,
            BlockFlags_Is_Solid
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

    std::unordered_map< glm::ivec2, Chunk*, Chunk_Hash > World::loaded_chunks;
    std::string World::path;
    i32 World::seed;

    std::mutex World::chunk_pool_mutex;
    Free_List<Chunk, World::chunk_capacity> World::chunk_pool;
    std::vector<Update_Sub_Chunk_Job> World::update_sub_chunk_jobs;

    u16 World::block_to_place_id = BlockId_Stone;

}