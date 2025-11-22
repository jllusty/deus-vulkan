// time.hpp: global structures (I know, bite me) for timing and
//           utility functions for getting the current duration
#pragma once

#include <chrono>

#include "core/types.hpp"

namespace core::time {

using Clock = std::chrono::steady_clock;

// stores the const global for the start time, lazily initialized for now
struct GlobalTime {
    inline static Clock::time_point start = Clock::now();
};

// get timestamp - nanoseconds
[[nodiscard]] inline core::u64 getTimestamp() noexcept {
    const Clock::time_point now = Clock::now();
    const std::chrono::duration dt = now - GlobalTime::start;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(dt).count();
}

// minutes, seconds, millis
struct MSMTime {
    core::u32 minutes{ 0 };
    core::u32 seconds{ 0 };
    core::u32 millis{ 0 };
};

// get minutes, seconds, millis from nanos
[[nodiscard]] inline const MSMTime getMSM(const core::u64& timestamp) noexcept {
    const std::chrono::nanoseconds ns{timestamp};

    const core::u32 mins = std::chrono::duration_cast<std::chrono::minutes>(ns).count();
    const core::u32 secs = std::chrono::duration_cast<std::chrono::seconds>(ns).count() % 60;
    const core::u32 mils = std::chrono::duration_cast<std::chrono::milliseconds>(ns).count() % 1000;

    return {mins, secs, mils};
}

}
