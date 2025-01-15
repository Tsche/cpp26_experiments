#pragma once
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <span>

#include "message/view.hpp"

namespace slo {

template <typename MessageBuffer>
struct MessageWriter {
  MessageBuffer buffer;
  std::size_t cursor = 0;

  void write_n(void* data, std::size_t n) {
    // todo use std::span?
    buffer.write(data, n);
    cursor += n;
  }

  void reserve(std::size_t n) { buffer.reserve(n); }

  [[nodiscard]] std::span<char const> finalize() const { return buffer.finalize(); }
};

template <typename MessageBuffer>
struct MessageReader {
  MessageBuffer buffer{};
  std::size_t cursor = 0;

  std::span<char const> get_n(std::size_t n) {
    auto data = buffer.read(n, cursor);
    cursor += n;
    return data;
  }
};

template <std::size_t N>
MessageReader(std::span<char const, N>) -> MessageReader<message::MessageView>;

}  // namespace slo