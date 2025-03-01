#pragma once
#include <cstdint>
#include <string>
#include <ctime>
#include <chrono>

#include <erl/thread.hpp>

namespace erl::logging {
enum class EventKind : std::uint8_t { LOGGING, THREAD_INFO };

enum class ThreadEvent : std::uint8_t { SPAWN, EXIT, RENAME, SET_PARENT };

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

using timestamp_t = std::chrono::system_clock::time_point;

enum Severity : std::uint8_t { DEBUG, INFO, WARNING, ERROR, FATAL };

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
  long long timestamp;

  // todo deserialize pointer to function
  // std::uintptr_t handler_ptr;
  formatter_type handler;
};

struct Message {
  Severity severity;
  CachedThreadInfo thread;
  timestamp_t timestamp;
  Location location;

  std::string text;
};
}  // namespace erl::logging