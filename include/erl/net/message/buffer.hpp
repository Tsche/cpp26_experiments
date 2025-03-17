#pragma once
#include <cassert>
#include <new>
#include <vector>
#include <span>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cstdint>


namespace erl::message {
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
  [[nodiscard]] explicit operator std::span<char const>() const { return buffer; }

  char* current(){
    return buffer.data() + buffer.size();
  }

private:
  std::vector<char> buffer;
};

template <std::size_t inline_capacity = std::hardware_destructive_interference_size - 4>
class HybridBuffer {
public:
  // HybridBuffer() { 
    // std::memset(storage.buffer, 0, sizeof(storage.buffer));
  // }
  HybridBuffer() = default;

  HybridBuffer(HybridBuffer const& other) : cursor(other.cursor) {
    if (other.is_heap()) {
      auto* new_ptr = static_cast<char*>(std::malloc(other.storage.heap.capacity));
      // assume allocating memory always works
      // assert(new_ptr);
      storage.heap = {new_ptr, other.storage.heap.capacity};
      std::memcpy(storage.heap.ptr, other.storage.heap.ptr, other.size());
    } else {
      std::memcpy(storage.buffer, other.storage.buffer, other.size());
    }
  }

  HybridBuffer(HybridBuffer&& other) noexcept : cursor(other.cursor) {
    if (other.is_heap()) {
      storage.heap = {other.storage.heap.ptr, other.storage.heap.capacity};
      other.storage.heap = {nullptr, 0};
    } else {
      std::memcpy(storage.buffer, other.storage.buffer, other.size());
    }
    other.cursor = 0;
  }

  HybridBuffer& operator=(HybridBuffer const& other) {
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

  void write(void const* input_data, unsigned length) {
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
        return;
      } else {
        // transition to heap once inline buffer is exhausted
        allocate_heap(std::max(cursor + length, static_cast<std::uint32_t>(inline_capacity) * 2));
      }
    }
    std::memcpy(storage.heap.ptr + size(), input_data, length);
    cursor -= length;
  }

  void write(std::span<char const> data){
    write(data.data(), data.size());
  }

  void reserve(unsigned num_bytes) {
    auto new_size = size() + num_bytes;
    if (is_heap()) {
      resize_heap(new_size);
    } else if (new_size > inline_capacity) {
      // transition to heap
      allocate_heap(new_size);
    }
  }

  [[nodiscard]] std::span<char const> read(std::size_t n, std::size_t offset = 0) { return {data() + offset, n}; }

  [[nodiscard]] std::span<char const> finalize() const { return {data(), size()}; }
  [[nodiscard]] explicit operator std::span<char const>() const { return {data(), size()}; }

  [[nodiscard]] char const* data() const {
    return is_heap() ? static_cast<char const*>(storage.heap.ptr)
                     : static_cast<char const*>(storage.buffer);
  }

  [[nodiscard]] std::size_t size() const { return is_heap() ? -cursor - 1 : cursor; }
  [[nodiscard]] bool is_empty() const { return size() == 0; }
  [[nodiscard]] bool is_heap() const { return cursor < 0; }

  char* current(){
    return data() + cursor;
  }
private:
  union Storage {
    char buffer[inline_capacity]{};
    struct HeapBuffer {
      char* ptr              = nullptr;
      std::uint32_t capacity = 0;
    } heap;

    Storage() { std::memset(buffer, 0, sizeof(buffer)); }
  };
  Storage storage;
  // sign indicates whether the internal buffer is used or not
  std::int32_t cursor{0};

  void allocate_heap(unsigned initial_capacity) {
    char* new_ptr = static_cast<char*>(std::malloc(initial_capacity));
    // assume allocating always works
    assert(new_ptr);
    std::memcpy(new_ptr, storage.buffer, size());
    storage.heap = {new_ptr, initial_capacity};

    if (cursor >= 0) {
      // -1 => heap at cursor 0
      cursor = -cursor - 1;
    }
  }

  void resize_heap(std::uint32_t new_capacity) {
    char* new_ptr = static_cast<char*>(std::realloc(storage.heap.ptr, new_capacity));
    // assume reallocating always works
    assert(new_ptr);
    storage.heap = {new_ptr, new_capacity};
  }
};

}  // namespace erl::message