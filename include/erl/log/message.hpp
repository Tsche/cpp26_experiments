#pragma once
#include <span>
#include <cstdint>
#include <string>
#include <ctime>
#include <chrono>

#include <erl/clock.hpp>
#include <erl/thread.hpp>

namespace erl::logging {
struct Location {
  char const* file{""};
  char const* function{""};
  std::uint32_t line{};
  std::uint32_t column{};
};

struct CachedThreadInfo {
  std::uint64_t id     = 0;
  std::string name     = "<unnamed>";
  std::uint64_t parent = 0;
};

enum Severity : std::uint8_t { TRACE, DEBUG, INFO, WARN, ERROR, FATAL };

namespace impl {
  struct FormattingResult {
    Location location;
    std::string text;
  };
}

using formatter_type = impl::FormattingResult (*)(std::span<char const>);

struct LoggingEvent {
  Severity severity;
  ThreadInfo thread;
  timestamp_t timestamp;
  formatter_type handler;
};

struct Message {
  Severity severity;
  CachedThreadInfo thread;
  timepoint_t timestamp;
  Location location;

  std::string text;
};
}  // namespace erl::logging