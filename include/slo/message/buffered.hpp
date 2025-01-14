#pragma once
#include <new>
#include <vector>
#include <span>
#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace slo::message {

struct HeapBuffer {
  std::span<char> read(std::size_t n, std::size_t offset = 0) { return {buffer.data() + offset, n}; }
  HeapBuffer& write(void const* data, std::size_t n) {
    copy(static_cast<char const*>(data), static_cast<char const*>(data) + n, back_inserter(buffer));
    return *this;
  }

  HeapBuffer& reserve(std::size_t n) {
    buffer.reserve(n);
    return *this;
  }

  std::span<char> finalize() { return buffer; }

private:
  std::vector<char> buffer;
};

template <std::size_t inline_capacity = std::hardware_destructive_interference_size>
class HybridBuffer {
public:
  HybridBuffer() { std::memset(storage.buffer, 0, sizeof(storage.buffer)); }

  HybridBuffer(const HybridBuffer& other) {
    if (other.cursor <= inline_capacity) {
      std::memcpy(storage.buffer, other.storage.buffer, other.cursor);
    } else {
      storage.heap.ptr = static_cast<char*>(std::malloc(other.storage.heap.capacity));
      // assume allocating memory always works
      storage.heap.capacity = other.storage.heap.capacity;
      std::memcpy(storage.heap.ptr, other.storage.heap.ptr, other.cursor);
    }
  }

  HybridBuffer(HybridBuffer&& other) noexcept {
    if (other.cursor <= inline_capacity) {
      std::memcpy(storage.buffer, other.storage.buffer, other.cursor);
    } else {
      storage.heap.ptr       = other.storage.heap.ptr;
      storage.heap.capacity  = other.storage.heap.capacity;
      other.storage.heap.ptr = nullptr;
      other.cursor           = 0;
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
    if (cursor > inline_capacity) {
      std::free(storage.heap.ptr);
    }
  }

  HybridBuffer& write(void const* input_data, unsigned length) {
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

  HybridBuffer& reserve(unsigned num_bytes) {
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

  std::span<char> finalize() { return std::span(data(), size()); }

  [[nodiscard]] char const* data() const {
    return cursor <= inline_capacity ? static_cast<char const*>(storage.buffer)
                                     : static_cast<char const*>(storage.heap.ptr);
  }

  [[nodiscard]] std::size_t size() const { return cursor; }

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

}