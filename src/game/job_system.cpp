#include "job_system.h"
#include "world.h"

namespace minecraft {

    static void execute_jobs(u32 thread_index)
    {
        auto& work_mutex = Job_System::internal_data.work_mutex;
        auto& work_cv = Job_System::internal_data.work_cv;

        Job_Queue* high_priority_queue = &Job_System::internal_data.high_priority_queue;
        Job_Queue* low_priority_queue  = &Job_System::internal_data.low_priority_queue;
        bool& running = Job_System::internal_data.running;

        Memory_Arena *arena = &Job_System::internal_data.arenas[thread_index];

        while (running || !high_priority_queue->is_empty() || !low_priority_queue->is_empty())
        {
            {
                std::unique_lock lock(work_mutex);
                work_cv.wait(lock, [&] { return !running || !high_priority_queue->is_empty() || !low_priority_queue->is_empty(); });
            }

            i32 job_index = high_priority_queue->job_index;
            i32 next_job_index = high_priority_queue->job_index + 1;
            if (next_job_index == MC_MAX_JOB_COUNT_PER_QUEUE) next_job_index = 0;

            Job *high_priority_job = nullptr;

            if (high_priority_queue->job_index != high_priority_queue->tail_job_index)
            {
                if (high_priority_queue->job_index.compare_exchange_strong(job_index, next_job_index))
                {
                    Temprary_Memory_Arena temp_arena = begin_temprary_memory_arena(arena);
                    Job *job = high_priority_queue->jobs + job_index;
                    job->execute(job->data, &temp_arena);
                    high_priority_job = job;
                    end_temprary_memory_arena(&temp_arena);
                }
            }

            if (!high_priority_job)
            {
                i32 job_index = low_priority_queue->job_index;
                i32 next_job_index = low_priority_queue->job_index + 1;
                if (next_job_index == MC_MAX_JOB_COUNT_PER_QUEUE) next_job_index = 0;

                if (low_priority_queue->job_index != low_priority_queue->tail_job_index)
                {
                    if (low_priority_queue->job_index.compare_exchange_strong(job_index, next_job_index))
                    {
                        Temprary_Memory_Arena temp_arena = begin_temprary_memory_arena(arena);
                        Job* job = low_priority_queue->jobs + job_index;
                        job->execute(job->data, &temp_arena);
                        end_temprary_memory_arena(&temp_arena);
                    }
                }
            }
        }
    }

    static void do_light_thread_work(World *world, Memory_Arena *arena)
    {
        auto& light_propagation_queue        = world->light_propagation_queue;
        auto& calculate_chunk_lighting_queue = world->calculate_chunk_lighting_queue;
        auto& update_chunk_jobs_queue        = world->update_chunk_jobs_queue;

        auto *light_queue = ArenaPushAligned(arena, Circular_Queue< Block_Query_Result >);
        Assert(light_queue);
        light_queue->initialize();

        while (Job_System::internal_data.running)
        {
            while (!light_propagation_queue.is_empty())
            {
                Calculate_Chunk_Light_Propagation_Job job = light_propagation_queue.pop();
                Chunk *chunk = job.chunk;
                propagate_sky_light(world, chunk, light_queue);
                chunk->state = ChunkState_LightPropagated;
            }

            while (!calculate_chunk_lighting_queue.is_empty())
            {
                Calculate_Chunk_Lighting_Job job = calculate_chunk_lighting_queue.pop();
                Chunk *chunk = job.chunk;
                calculate_lighting(world, chunk, light_queue);
                chunk->state = ChunkState_LightCalculated;
            }

            while (!light_queue->is_empty())
            {
                auto block_query = light_queue->pop();

                Block* block = block_query.block;
                Block_Light_Info *block_light_info = get_block_light_info(block_query.chunk, block_query.block_coords);
                const Block_Info* info = get_block_info(world, block);
                auto neighbours_query = query_neighbours(block_query.chunk, block_query.block_coords);

                for (i32 d = 0; d < 6; d++)
                {
                    auto &neighbour_query = neighbours_query[d];

                    if (is_block_query_valid(neighbour_query) &&
                        is_block_query_in_world_region(neighbour_query, world->active_region_bounds))
                    {
                        Block* neighbour = neighbour_query.block;
                        const auto* neighbour_info = get_block_info(world, neighbour);
                        if (is_block_transparent(neighbour_info))
                        {
                            Block_Light_Info *neighbour_block_light_info = get_block_light_info(neighbour_query.chunk, neighbour_query.block_coords);

                            if ((i32)neighbour_block_light_info->sky_light_level <= (i32)block_light_info->sky_light_level - 2)
                            {
                                set_block_sky_light_level(world, neighbour_query.chunk, neighbour_query.block_coords, (i32)block_light_info->sky_light_level - 1);
                                light_queue->push(neighbour_query);
                            }

                            if ((i32)neighbour_block_light_info->light_source_level <= (i32)block_light_info->light_source_level - 2)
                            {
                                set_block_light_source_level(world, neighbour_query.chunk, neighbour_query.block_coords, (i32)block_light_info->light_source_level - 1);
                                light_queue->push(neighbour_query);
                            }
                        }
                    }
                }
            }

            while (!update_chunk_jobs_queue.is_empty())
            {
                auto job = update_chunk_jobs_queue.pop();
                Job_System::schedule(job);
            }
        }
    }

    bool Job_System::initialize(World *world, Memory_Arena *permanent_arena)
    {
        u32 concurrent_thread_count = std::thread::hardware_concurrency();

        if (concurrent_thread_count == 1)
        {
            fprintf(stderr, "[ERROR]: can't run game on low thread count\n");
            return false;
        }

        internal_data.thread_count = concurrent_thread_count - 2;
        if (internal_data.thread_count > MC_MAX_THREAD_COUNT) internal_data.thread_count = MC_MAX_THREAD_COUNT;

        internal_data.high_priority_queue.job_index = 0;
        internal_data.high_priority_queue.tail_job_index = 0;
        memset(internal_data.high_priority_queue.jobs, 0, sizeof(Job) * MC_MAX_JOB_COUNT_PER_QUEUE);

        internal_data.low_priority_queue.job_index = 0;
        internal_data.low_priority_queue.tail_job_index = 0;
        memset(internal_data.low_priority_queue.jobs, 0, sizeof(Job) * MC_MAX_JOB_COUNT_PER_QUEUE);

        internal_data.running = true;

        for (u32 i = 0; i < internal_data.thread_count; i++)
        {
            // todo(harlequin): maybe we can use thread local storage
            internal_data.arenas[i]  = push_sub_arena(permanent_arena, MegaBytes(1));
            internal_data.threads[i] = std::thread(execute_jobs, i);
        }

        internal_data.light_thread = std::thread(do_light_thread_work, world, permanent_arena);
        return true;
    }

    void Job_System::shutdown()
    {
        auto& work_mutex = Job_System::internal_data.work_mutex;
        auto& work_cv = Job_System::internal_data.work_cv;

        internal_data.running = false;

        {
            std::unique_lock lock(work_mutex);
            work_cv.notify_all();
        }

        for (u32 thread_index = 0; thread_index < internal_data.thread_count; thread_index++)
        {
            internal_data.threads[thread_index].join();
        }

        internal_data.light_thread.join();
    }

    void Job_System::dispatch(const Job& job, bool high_prority)
    {
        Job_Queue *queue = high_prority ? &internal_data.high_priority_queue : &internal_data.low_priority_queue;
        i32 tail_job_index = queue->tail_job_index;
        i32 next_tail_job_index = tail_job_index + 1;
        if (next_tail_job_index == MC_MAX_JOB_COUNT_PER_QUEUE) next_tail_job_index = 0;
        queue->jobs[tail_job_index] = job;
        queue->tail_job_index = next_tail_job_index;
    }

    void Job_System::wait_for_jobs_to_finish()
    {
        Job_Queue* high_priority_queue = &internal_data.high_priority_queue;
        Job_Queue* low_priority_queue  = &internal_data.low_priority_queue;

        while (!high_priority_queue->is_empty() || !low_priority_queue->is_empty())
        {
            // execute_jobs();
        }
    }

    Job_System_Data Job_System::internal_data;
}