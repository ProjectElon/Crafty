#pragma once

#include "core/common.h"

#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

namespace minecraft {

    #define MC_MAX_THREAD_COUNT 64
    #define MC_MAX_JOB_COUNT_PER_QUEUE 65536

    struct Job
    {
        void *data;
        void (*execute)(void *data);
    };

    struct Job_Queue
    {
        std::atomic<i32> job_index;
        std::atomic<i32> tail_job_index;
        Job jobs[MC_MAX_JOB_COUNT_PER_QUEUE];

        inline bool is_empty() { return job_index == tail_job_index; }
    };

    struct Job_System_Data
    {
        bool running;

        u32 thread_count;
        std::thread threads[MC_MAX_THREAD_COUNT];

        std::thread light_thread;

        std::mutex work_mutex;
        std::condition_variable work_cv;

        Job_Queue high_priority_queue;
        Job_Queue low_priority_queue;
    };

    struct Job_System
    {
        static Job_System_Data internal_data;

        static bool initialize();
        static void shutdown();

        static void dispatch(const Job& job, bool high_prority);

        template<typename T>
        static void schedule(const T& job_data, bool high_prority = true)
        {
            static std::vector<T> job_data_pool(MC_MAX_JOB_COUNT_PER_QUEUE);
            static u32 job_data_index = 0;

            bool should_notifiy_worker_threads = internal_data.high_priority_queue.is_empty() && internal_data.low_priority_queue.is_empty();

            std::unique_lock lock(internal_data.work_mutex);
            job_data_pool[job_data_index] = job_data;

            Job job;
            job.data    = &job_data_pool[job_data_index];
            job.execute = &T::execute;
            dispatch(job, high_prority);

            job_data_index++;
            if (job_data_index == MC_MAX_JOB_COUNT_PER_QUEUE) job_data_index = 0;

            if (should_notifiy_worker_threads)
            {
                internal_data.work_cv.notify_all();
            }
        }

        static void wait_for_jobs_to_finish();
    };
}