#pragma once

#include "core/common.h"

#include <thread>
#include <mutex>
#include <condition_variable>

namespace minecraft {

    #define MC_MAX_THREAD_COUNT 64
    #define MC_MAX_JOB_COUNT_PER_PATCH 512

    struct Job
    {
        void *data;
        void (*execute)(void *data);
    };

    struct Job_Queue
    {
        u32 job_index;
        u32 tail_job_index;
        Job jobs[MC_MAX_JOB_COUNT_PER_PATCH];
    };

    struct Job_System_Data
    {
        bool running;

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
            static std::vector<T> jobs_data(MC_MAX_THREAD_COUNT * MC_MAX_JOB_COUNT_PER_PATCH);
            static u32 job_data_index = 0;

            jobs_data[job_data_index] = job_data;

            Job job;
            job.data = &jobs_data[job_data_index];
            job.execute = &T::execute;
            dispatch(job);

            job_data_index++;
            if (job_data_index == MC_MAX_THREAD_COUNT * MC_MAX_JOB_COUNT_PER_PATCH) job_data_index = 0;
        }
    };
}