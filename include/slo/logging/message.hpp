#pragma once
#include <cstdint>
#include <string>

namespace slo::logging {
struct Location {
  char const* file{};
  char const* function{};
  std::uint32_t line{};
  std::uint32_t column{};
};

struct Message {
  std::string text;
  Location location{};
  std::uint64_t thread{};
};
}  // namespace slo::logging