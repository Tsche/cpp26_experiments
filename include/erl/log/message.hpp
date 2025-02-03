#pragma once
#include <cstdint>
#include <string>
#include <ctime>
#include <erl/threading/info.hpp>

namespace erl::logging {
struct Location {
  char const* file{};
  char const* function{};
  std::uint32_t line{};
  std::uint32_t column{};
};

enum Severity : std::uint8_t {
  DEBUG,
  INFO,
  WARNING,
  ERROR,
  FATAL
};

struct Prelude {
  Severity severity;
  std::time_t timestamp;
  ThreadInfo thread;
};

struct Message {
  Prelude meta;
  std::string text;
  Location location;
};
}  // namespace erl::logging