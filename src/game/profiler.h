#pragma once

#include "core/common.h"

namespace minecraft {

    struct Profile
    {
        const char* name;
        f64 elapsed_time;
    };

    struct Profile_Timer
    {
        f64 start_time;
        Profile profile;

        Profile_Timer(const char *name);
        ~Profile_Timer();
    };

    #define PROFILE_FUNCTION Profile_Timer COUNTER##_timer(__FUNCTION__)
    #define PROFILE_BLOCK(Name) Profile_Timer COUNTER##_timer(Name)

    struct Profiler_Data
    {
        std::vector<Profile> profiles;
        f64 start_time;
        f64 target_frame_rate;
    };

    struct Profiler
    {
        static Profiler_Data internal_data;

        static bool initialize(u32 target_frame_rate);

        static void begin();
        static void end();
    };
}
