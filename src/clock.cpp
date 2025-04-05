#include <erl/time>

namespace erl {
timestamp_t current_time(){
  return std::chrono::system_clock::now().time_since_epoch().count();
}

timepoint_t to_timepoint(timestamp_t timestamp) {
  return timepoint_t{std::chrono::system_clock::duration{timestamp}};
}
}