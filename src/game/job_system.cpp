#include "job_system.h"
#include "world.h"

namespace minecraft {

    static void execute_jobs()
    {
        auto& work_mutex = Job_System::internal_data.work_mutex;
        auto& work_cv = Job_System::internal_data.work_cv;

        Job_Queue* high_priority_queue = &Job_System::internal_data.high_priority_queue;
        Job_Queue* low_priority_queue  = &Job_System::internal_data.low_priority_queue;
        bool& running = Job_System::internal_data.running;

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
                    Job *job = high_priority_queue->jobs + job_index;
                    job->execute(job->data);
                    high_priority_job = job;
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
                        Job* job = low_priority_queue->jobs + job_index;
                        job->execute(job->data);
                    }
                }
            }
        }
    }

    static void do_light_thread_work()
    {
        auto& light_propagation_queue        = World::light_propagation_queue;
        auto& calculate_chunk_lighting_queue = World::calculate_chunk_lighting_queue;

        auto& update_chunk_jobs_queue = World::update_chunk_jobs_queue;

        Circular_FIFO_Queue<Block_Query_Result> light_queue;
        light_queue.initialize();

        while (Job_System::internal_data.running)
        {
            while (!light_propagation_queue.is_empty())
            {
                Calculate_Chunk_Light_Propagation_Job job = light_propagation_queue.pop();
                Chunk *chunk = job.chunk;
                chunk->propagate_sky_light(&light_queue);
                chunk->light_propagated = true;
                chunk->in_light_propagation_queue = false;
            }

            while (!calculate_chunk_lighting_queue.is_empty())
            {
                Calculate_Chunk_Lighting_Job job = calculate_chunk_lighting_queue.pop();
                Chunk *chunk = job.chunk;
                chunk->calculate_lighting(&light_queue);
                chunk->in_light_calculation_queue = false;
            }

            while (!light_queue.is_empty())
            {
                auto block_query = light_queue.pop();

                Block* block = block_query.block;
                Block_Light_Info *block_light_info = block_query.chunk->get_block_light_info(block_query.block_coords);
                const auto& info = block->get_info();
                auto neighbours_query = World::get_neighbours(block_query.chunk, block_query.block_coords);

                for (i32 d = 0; d < 6; d++)
                {
                    auto& neighbour_query = neighbours_query[d];

                    if (World::is_block_query_valid(neighbour_query) &&
                        World::is_block_query_in_world_region(neighbour_query, World::player_region_bounds))
                    {
                        Block* neighbour = neighbour_query.block;
                        const auto& neighbour_info = neighbour->get_info();
                        if (neighbour_info.is_transparent())
                        {
                            Block_Light_Info *neighbour_block_light_info = neighbour_query.chunk->get_block_light_info(neighbour_query.block_coords);

                            if ((i32)neighbour_block_light_info->sky_light_level <= (i32)block_light_info->sky_light_level - 2)
                            {
                                World::set_block_sky_light_level(neighbour_query.chunk, neighbour_query.block_coords, (i32)block_light_info->sky_light_level - 1);
                                light_queue.push(neighbour_query);
                            }

                            if ((i32)neighbour_block_light_info->light_source_level <= (i32)block_light_info->light_source_level - 2)
                            {
                                World::set_block_light_source_level(neighbour_query.chunk, neighbour_query.block_coords, (i32)block_light_info->light_source_level - 1);
                                light_queue.push(neighbour_query);
                            }
                        }
                    }
                }
            }

            auto& update_chunk_jobs_queue = World::update_chunk_jobs_queue;

            while (!update_chunk_jobs_queue.is_empty())
            {
                auto job = update_chunk_jobs_queue.pop();
                Job_System::schedule(job);
            }
        }
    }

    bool Job_System::initialize()
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
            internal_data.threads[i] = std::thread(execute_jobs);
        }

        internal_data.light_thread = std::thread(do_light_thread_work);
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