#pragma once
#include <cstdint>
#include <chrono>

namespace erl {
using timestamp_t = std::uint64_t;
using timepoint_t = std::chrono::system_clock::time_point;

timestamp_t current_time();
timepoint_t to_timepoint(timestamp_t timestamp);
}