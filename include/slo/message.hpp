#pragma once
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <span>

#include "message/view.hpp"
#include "reflect.hpp"

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

  std::span<char> finalize() { return buffer.finalize(); }
};

template <typename MessageBuffer>
struct MessageReader {
  MessageBuffer buffer{};
  std::size_t cursor = 0;

  std::span<char> get_n(std::size_t n) {
    auto data = buffer.read(n, cursor);
    cursor += n;
    return data;
  }
};

template <std::size_t N>
MessageReader(std::span<char, N>) -> MessageReader<message::MessageView>;

// template <std::size_t Extent = std::dynamic_extent>
// struct [[deprecated]] MessageParser {
//   std::span<char const, Extent> payload;
//   unsigned cursor = 0;

//   char const* current() { return payload.data() + cursor; }

//   bool has_n(unsigned num_bytes) { return cursor + num_bytes <= payload.size(); }

//   template <typename T>
//   constexpr decltype(auto) get() {
//     return Reflect<T>::deserialize(*this);
//   }
// };

}  // namespace slo