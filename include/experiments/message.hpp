#pragma once
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <span>
#include <experimental/meta>

#include "reflect.hpp"

namespace rpc {
template <typename T>
struct Reflect;

template <std::size_t inline_capacity = std::hardware_destructive_interference_size>
class MessageWriter {
public:
  MessageWriter() { std::memset(storage.buffer, 0, sizeof(storage.buffer)); }

  MessageWriter(const MessageWriter& other) {
    if (other.cursor <= inline_capacity) {
      std::memcpy(storage.buffer, other.storage.buffer, other.cursor);
    } else {
      storage.heap.ptr = static_cast<char*>(std::malloc(other.storage.heap.capacity));
      // assume allocating memory always works
      storage.heap.capacity = other.storage.heap.capacity;
      std::memcpy(storage.heap.ptr, other.storage.heap.ptr, other.cursor);
    }
  }

  MessageWriter(MessageWriter&& other) noexcept {
    if (other.cursor <= inline_capacity) {
      std::memcpy(storage.buffer, other.storage.buffer, other.cursor);
    } else {
      storage.heap.ptr       = other.storage.heap.ptr;
      storage.heap.capacity  = other.storage.heap.capacity;
      other.storage.heap.ptr = nullptr;
      other.cursor           = 0;
    }
  }

  MessageWriter& operator=(const MessageWriter& other) {
    if (this != &other) {
      this->~MessageWriter();
      new (this) MessageWriter(other);
    }
    return *this;
  }

  MessageWriter& operator=(MessageWriter&& other) noexcept {
    if (this != &other) {
      this->~MessageWriter();
      new (this) MessageWriter(std::move(other));
    }
    return *this;
  }

  ~MessageWriter() {
    if (cursor > inline_capacity) {
      std::free(storage.heap.ptr);
    }
  }

  MessageWriter& extend(const void* input_data, unsigned length) {
    if (cursor + length <= inline_capacity) {
      // use inline buffer
      std::memcpy(storage.buffer + cursor, input_data, length);
    } else {
      if (cursor <= inline_capacity) {
        // transition to heap once inline buffer is exhausted
        allocate_heap(std::max(cursor + length, inline_capacity * 2));
      } else if (cursor + length > storage.heap.capacity) {
        // resize if necessary
        resize_heap(cursor + length);
      }
      std::memcpy(storage.heap.ptr + cursor, input_data, length);
    }
    cursor += length;
    return *this;
  }

  MessageWriter& reserve(unsigned num_bytes) {
    if (cursor + num_bytes <= inline_capacity) {
      return *this;
    }
    if (cursor <= inline_capacity) {
      allocate_heap(cursor + num_bytes);
    } else if (cursor + num_bytes > storage.heap.capacity) {
      resize_heap(cursor + num_bytes);
    }
    return *this;
  }

  auto payload() const { return std::span(data(), size()); }

  [[nodiscard]] char const* data() const {
    return cursor <= inline_capacity ? static_cast<char const*>(storage.buffer)
                                     : static_cast<char const*>(storage.heap.ptr);
  }

  [[nodiscard]] std::size_t size() const { return cursor; }

  template <typename T>
  MessageWriter& add_field(T&& arg) {
    Reflect<std::remove_cvref_t<T>>::serialize(std::forward<T>(arg), *this);
    return *this;
  }

private:
  union Storage {
    char buffer[inline_capacity];
    struct HeapBuffer {
      char* ptr              = nullptr;
      std::uint32_t capacity = 0;
    } heap;

    Storage() { std::memset(buffer, 0, sizeof(buffer)); }
  };
  std::size_t cursor{0};
  Storage storage;

  void allocate_heap(unsigned initial_capacity) {
    char* new_ptr = static_cast<char*>(std::malloc(initial_capacity));
    // assume allocating always works
    // assert(new_ptr);
    std::memcpy(new_ptr, storage.buffer, cursor);
    storage.heap = {new_ptr, initial_capacity};
  }

  void resize_heap(std::uint32_t new_capacity) {
    char* new_ptr = static_cast<char*>(std::realloc(storage.heap.ptr, new_capacity));
    // assume reallocating always works
    // assert(new_ptr);
    storage.heap = {new_ptr, new_capacity};
  }
};

template <std::size_t Extent = std::dynamic_extent>
struct MessageParser {
  std::span<char const, Extent> payload;
  unsigned cursor = 0;

  char const* current() { return payload.data() + cursor; }

  bool has_n(unsigned num_bytes) { return cursor + num_bytes <= payload.size(); }

  template <typename T>
  constexpr decltype(auto) get() {
    return Reflect<T>::deserialize(*this);
  }
};

struct Message {
  enum Flags : std::uint8_t { RET = 0U, REQ = 1U << 1U, FNC = 1U << 2U, ONEWAY = 1U << 3U };

  Flags kind;
  std::uint8_t opcode;
  std::uint16_t checksum;
  std::uint32_t size;

  static constexpr auto inline_capacity =
      std::hardware_destructive_interference_size - (sizeof(kind) + sizeof(opcode) + sizeof(checksum) + sizeof(size));
  union {
    char buffer[inline_capacity];
    char* heap;
  } data;

  [[nodiscard]] std::span<char const> get_data() const {
    auto* payload = size > inline_capacity ? data.heap : data.buffer;
    return {payload, size};
  }

  [[nodiscard]] std::size_t get_index() const{
    return opcode;
  }
};
}  // namespace rpc