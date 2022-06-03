#include "job_system.h"

namespace minecraft {

    static void execute_jobs(bool *running, Job_Queue *high_priority_queue, Job_Queue *low_priority_queue)
    {
        while (*running || !high_priority_queue->is_empty() || !low_priority_queue->is_empty())
        {
            {
                std::unique_lock lock(Job_System::internal_data.work_mutex);
                Job_System::internal_data.work_cv.wait(lock, [&] { return !high_priority_queue->is_empty() || !low_priority_queue->is_empty(); });
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
            internal_data.threads[i] = std::thread(execute_jobs, &internal_data.running, &internal_data.high_priority_queue, &internal_data.low_priority_queue);
        }

        return true;
    }

    void Job_System::shutdown()
    {
        internal_data.running = false;

        for (u32 thread_index = 0; thread_index < internal_data.thread_count; thread_index++)
        {
            internal_data.threads[thread_index].join();
        }
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
        Job_Queue* low_priority_queue = &internal_data.low_priority_queue;

        while (!high_priority_queue->is_empty() || !low_priority_queue->is_empty())
        {
            // i32 job_index = high_priority_queue->job_index;
            // i32 next_job_index = high_priority_queue->job_index + 1;
            // if (next_job_index == MC_MAX_JOB_COUNT_PER_QUEUE) next_job_index = 0;

            // Job *high_priority_job = nullptr;

            // if (high_priority_queue->job_index != high_priority_queue->tail_job_index)
            // {
            //     if (high_priority_queue->job_index.compare_exchange_strong(job_index, next_job_index))
            //     {
            //         Job *job = high_priority_queue->jobs + job_index;
            //         job->execute(job->data);
            //         high_priority_job = job;
            //     }
            // }

            // if (!high_priority_job)
            // {
            //     i32 job_index = low_priority_queue->job_index;
            //     i32 next_job_index = low_priority_queue->job_index + 1;
            //     if (next_job_index == MC_MAX_JOB_COUNT_PER_QUEUE) next_job_index = 0;

            //     if (low_priority_queue->job_index != low_priority_queue->tail_job_index)
            //     {
            //         if (low_priority_queue->job_index.compare_exchange_strong(job_index, next_job_index))
            //         {
            //             Job* job = low_priority_queue->jobs + job_index;
            //             job->execute(job->data);
            //         }
            //     }
            // }
        }
    }

    Job_System_Data Job_System::internal_data;
}