#include "game/profiler.h"
#include "core/platform.h"

namespace minecraft {

    Profile_Timer::Profile_Timer(const char *name)
    {
        auto& platform = Game::get_platform();
        this->profile.name = name;
        this->start_time   = platform.get_current_time();
    }

    Profile_Timer::~Profile_Timer()
    {
        auto& platform = Game::get_platform();
        this->profile.elapsed_time = platform.get_current_time() - this->start_time;
        Profiler::internal_data.profiles.emplace_back(std::move(this->profile));
    }

    bool Profiler::initialize(u32 target_frame_rate)
    {
        internal_data.profiles.reserve(65536);
        internal_data.target_frame_rate = 1.0 / (f64)target_frame_rate;
        return true;
    }

    void Profiler::begin()
    {
        auto& platform = Game::get_platform();
        internal_data.start_time = platform.get_current_time();
    }

    void Profiler::end()
    {
        auto& platform = Game::get_platform();
        f64 delta_time = platform.get_current_time() - internal_data.start_time;
        if (delta_time >= internal_data.target_frame_rate)
        {
            fprintf(stderr, "================= frame drop %.2fms =================\n", delta_time * 1000.0f);

            for (auto& profile : internal_data.profiles)
            {
                fprintf(stderr, "%s ---- %.2fms\n", profile.name, profile.elapsed_time * 1000.0f);
            }

            fprintf(stderr, "=====================================================\n");
        }
        internal_data.profiles.resize(0);
    }

    Profiler_Data Profiler::internal_data;
}