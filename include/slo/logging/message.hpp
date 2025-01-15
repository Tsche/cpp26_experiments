#pragma once
#include <cstdint>
#include <string>
#include <ctime>
#include <slo/threading/info.hpp>

namespace slo::logging {
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
  std::uintptr_t formatter;
};

struct Message {
  Prelude meta;
  std::string text;
  Location location;
};
}  // namespace slo::logging