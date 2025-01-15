#pragma once
#include <new>
#include <vector>
#include <span>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cstdint>

namespace slo::message {

struct HeapBuffer {
  std::span<char const> read(std::size_t n, std::size_t offset = 0) { return {buffer.data() + offset, n}; }
  HeapBuffer& write(void const* data, std::size_t n) {
    copy(static_cast<char const*>(data), static_cast<char const*>(data) + n, back_inserter(buffer));
    return *this;
  }

  HeapBuffer& reserve(std::size_t n) {
    buffer.reserve(n);
    return *this;
  }

  std::span<char const> finalize() const { return buffer; }

private:
  std::vector<char> buffer;
};

template <std::size_t inline_capacity = std::hardware_destructive_interference_size>
class HybridBuffer {
public:
  HybridBuffer() { std::memset(storage.buffer, 0, sizeof(storage.buffer)); }

  HybridBuffer(const HybridBuffer& other) {
    if (other.is_heap()) {
      storage.heap.ptr = static_cast<char*>(std::malloc(other.storage.heap.capacity));
      // assume allocating memory always works
      storage.heap.capacity = other.storage.heap.capacity;
      std::memcpy(storage.heap.ptr, other.storage.heap.ptr, other.cursor);
    } else {
      std::memcpy(storage.buffer, other.storage.buffer, other.cursor);
    }
  }

  HybridBuffer(HybridBuffer&& other) noexcept {
    if (is_heap()) {
      storage.heap.ptr       = other.storage.heap.ptr;
      storage.heap.capacity  = other.storage.heap.capacity;
      other.storage.heap.ptr = nullptr;
      other.cursor           = 0;
    } else {
      std::memcpy(storage.buffer, other.storage.buffer, other.cursor);
    }
  }

  HybridBuffer& operator=(const HybridBuffer& other) {
    if (this != &other) {
      this->~HybridBuffer();
      new (this) HybridBuffer(other);
    }
    return *this;
  }

  HybridBuffer& operator=(HybridBuffer&& other) noexcept {
    if (this != &other) {
      this->~HybridBuffer();
      new (this) HybridBuffer(std::move(other));
    }
    return *this;
  }

  ~HybridBuffer() {
    if (is_heap()) {
      std::free(storage.heap.ptr);
    }
  }

  HybridBuffer& write(void const* input_data, unsigned length) {
    auto new_size = size() + length;
    if (is_heap()) {
      if (new_size > storage.heap.capacity) {
        // resize if necessary
        resize_heap(new_size);
      }
    } else {
      if (new_size <= inline_capacity) {
        // use inline buffer
        std::memcpy(storage.buffer + cursor, input_data, length);
        cursor += length;
        return *this;
      } else {
        // transition to heap once inline buffer is exhausted
        allocate_heap(std::max(cursor + length, (long long)inline_capacity * 2));
      }
    }
    std::memcpy(storage.heap.ptr + size(), input_data, length);
    cursor -= length;
    return *this;
  }

  HybridBuffer& reserve(unsigned num_bytes) {
    auto new_size = size() + num_bytes;
    if (is_heap()) {
      resize_heap(new_size);
    } else if (new_size > inline_capacity) {
      // transition to heap
      allocate_heap(new_size);
    }
    return *this;
  }

  [[nodiscard]] std::span<char const> read(std::size_t n, std::size_t offset = 0) { return {data() + offset, n}; }

  [[nodiscard]] std::span<char const> finalize() const { return std::span(data(), size()); }

  [[nodiscard]] char const* data() const {
    return is_heap() ? static_cast<char const*>(storage.heap.ptr)
                     : static_cast<char const*>(storage.buffer);
  }

  [[nodiscard]] std::size_t size() const { return is_heap() ? -cursor - 1 : cursor; }
  [[nodiscard]] bool is_heap() const { return cursor < 0; }

private:
  union Storage {
    char buffer[inline_capacity];
    struct HeapBuffer {
      char* ptr              = nullptr;
      std::uint32_t capacity = 0;
    } heap;

    Storage() { std::memset(buffer, 0, sizeof(buffer)); }
  };
  Storage storage;
  // sign indicates whether the internal buffer is used or not
  long long cursor{0};

  void allocate_heap(unsigned initial_capacity) {
    char* new_ptr = static_cast<char*>(std::malloc(initial_capacity));
    // assume allocating always works
    // assert(new_ptr);
    std::memcpy(new_ptr, storage.buffer, cursor);
    storage.heap = {new_ptr, initial_capacity};

    if (cursor >= 0) {
      // -1 => heap at cursor 0
      cursor = -cursor - 1;
    }
  }

  void resize_heap(std::uint32_t new_capacity) {
    char* new_ptr = static_cast<char*>(std::realloc(storage.heap.ptr, new_capacity));
    // assume reallocating always works
    // assert(new_ptr);
    storage.heap = {new_ptr, new_capacity};
  }
};

}  // namespace slo::message