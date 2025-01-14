#pragma once
#include <cstdint>
#include <span>

namespace rpc {
struct MessageHeader {
  enum Flags : std::uint8_t { RET = 0U, REQ = 1U << 1U, FNC = 1U << 2U, ONEWAY = 1U << 3U };

  Flags kind;
  std::uint8_t opcode;
  std::uint16_t checksum;
  std::uint32_t size;
};

struct Message {
  MessageHeader header;
  std::span<char> data;
};

}