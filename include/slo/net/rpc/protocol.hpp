#pragma once
#include <cstdint>
#include <span>

namespace slo::rpc {
struct Prelude {
  std::uint16_t endpoint;
  std::uint16_t function;
  std::uint32_t size; // TODO implicit size from function?
};

struct Message {
  Prelude header;
  std::span<char> data;
};

}