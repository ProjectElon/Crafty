#pragma once

#include "core/common.h"

#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

namespace minecraft {

    #define MC_MAX_THREAD_COUNT 64
    #define MC_MAX_JOB_COUNT_PER_PATCH 4096

    struct Job
    {
        void *data;
        void (*execute)(void *data);
    };

    struct Job_Queue
    {
        std::mutex work_condition_mutex;
        std::condition_variable work_condition;
        std::atomic<bool> running;
        std::atomic<u32> job_index;
        std::atomic<u32> tail_job_index;
        Job jobs[MC_MAX_JOB_COUNT_PER_PATCH];
    };

    struct Job_System_Data
    {
        u32 thread_count;
        std::thread threads[MC_MAX_THREAD_COUNT];

        u32 job_queue_index;
        Job_Queue queues[MC_MAX_THREAD_COUNT];
    };

    struct Job_System
    {
        static Job_System_Data internal_data;

        static bool initialize();
        static void shutdown();

        static void dispatch(const Job& job);

        template<typename T>
        static void schedule(const T& job_data)
        {
            static std::vector<T> job_data_pool(MC_MAX_THREAD_COUNT * MC_MAX_JOB_COUNT_PER_PATCH);
            static u32 job_data_index = 0;

            job_data_pool[job_data_index] = job_data;

            Job job;
            job.data = &job_data_pool[job_data_index];
            job.execute = &T::execute;
            dispatch(job);

            job_data_index++;
            if (job_data_index == MC_MAX_THREAD_COUNT * MC_MAX_JOB_COUNT_PER_PATCH) job_data_index = 0;
        }

        static void wait_for_jobs_to_finish();
    };
}