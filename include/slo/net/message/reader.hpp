#pragma once
#include <cassert>
#include <concepts>
#include <new>
#include <span>
#include "buffer.hpp"


namespace slo::message {
struct MessageView {
  std::span<char const> buffer;
  std::size_t cursor = 0;

  std::span<char const> read(std::size_t n) {
    // assert(cursor + n < buffer.size());
    auto ret = std::span<char const>{buffer.data() + cursor, n};
    cursor += n;
    return ret; 
  }
};

template <typename T>
concept is_stream = requires (T obj) {
  {obj.send(std::span<char const>{})} -> std::same_as<void>;
  {obj.receive(nullptr, 1)} -> std::same_as<void>;
};

template <is_stream T>
struct MessageStream {
  static constexpr auto buffer_size = std::hardware_destructive_interference_size - sizeof(T*);

  T* interface;
  HybridBuffer<buffer_size> buffer;

  std::span<char const> read(std::size_t n) { 
    buffer.reserve(n);
    interface->receive(buffer.current(), n);
  }
};
}  // namespace slo::message