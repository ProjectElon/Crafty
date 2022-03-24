#include "job_system.h"

namespace minecraft {

    static void execute_jobs(Job_Queue *queue)
    {
        while (Job_System::internal_data.running || queue->job_index != queue->tail_job_index)
        {
            if (queue->job_index != queue->tail_job_index)
            {
                Job& job = queue->jobs[queue->job_index];
                job.execute(job.data);

                queue->job_index++;
                if (queue->job_index == MC_MAX_JOB_COUNT_PER_PATCH) queue->job_index = 0;
            }
        }
    }

    bool Job_System::initialize()
    {
        u32 concurrent_thread_count = std::thread::hardware_concurrency();
        if (concurrent_thread_count == 2) return false;

        internal_data.thread_count = concurrent_thread_count - 2;
        if (internal_data.thread_count > MC_MAX_THREAD_COUNT) internal_data.thread_count = MC_MAX_THREAD_COUNT;

        internal_data.running = true;
        internal_data.job_queue_index = 0;

        for (u32 queue_index = 0; queue_index < internal_data.thread_count; queue_index++)
        {
            Job_Queue* queue = internal_data.queues + queue_index;
            queue->job_index = 0;
            queue->tail_job_index = 0;
            memset(queue->jobs, 0, sizeof(Job) * MC_MAX_JOB_COUNT_PER_PATCH);

            internal_data.threads[queue_index] = std::thread(execute_jobs, queue);
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

    void Job_System::dispatch(const Job& job)
    {
        Job_Queue* queue = internal_data.queues + internal_data.job_queue_index;

        queue->jobs[queue->tail_job_index] = job;
        queue->tail_job_index++;
        if (queue->tail_job_index == MC_MAX_JOB_COUNT_PER_PATCH) queue->tail_job_index = 0;

        internal_data.job_queue_index++;
        if (internal_data.job_queue_index == internal_data.thread_count) internal_data.job_queue_index = 0;
    }

    void Job_System::wait_for_jobs_to_finish()
    {
        for (u32 queue_index = 0; queue_index < MC_MAX_THREAD_COUNT; ++queue_index)
        {
            Job_Queue &queue = internal_data.queues[queue_index];
            while (queue.job_index != queue.tail_job_index);
        }
    }

    Job_System_Data Job_System::internal_data;
}