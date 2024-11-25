#pragma once
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <span>

namespace rpc {
template <typename T> struct Reflect;

class Message {
public:
  enum Flags : std::uint8_t {
    RET = 0,
    REQ = 1 << 1,
    FNC = 1 << 2,
    ONEWAY = 1 << 3
  };

  struct Header {
    Flags kind;
    std::uint8_t opcode;
    std::uint16_t checksum;
    std::uint32_t size;
  };

  explicit Message(Header header_) : header{header_} {
    std::memset(storage.buffer, 0, sizeof(storage.buffer));
  }

  Message() : Message(Header{}) {}

  Message(const Message &other) : header(other.header) {
    if (other.header.size <= inline_capacity) {
      std::memcpy(storage.buffer, other.storage.buffer, other.header.size);
    } else {
      storage.heap.ptr =
          static_cast<char *>(std::malloc(other.storage.heap.capacity));
      // assume allocating memory always works
      storage.heap.capacity = other.storage.heap.capacity;
      std::memcpy(storage.heap.ptr, other.storage.heap.ptr, other.header.size);
    }
  }

  Message(Message &&other) noexcept : header(other.header) {
    if (other.header.size <= inline_capacity) {
      std::memcpy(storage.buffer, other.storage.buffer, other.header.size);
    } else {
      storage.heap.ptr = other.storage.heap.ptr;
      storage.heap.capacity = other.storage.heap.capacity;
      other.storage.heap.ptr = nullptr;
      other.header.size = 0;
    }
  }

  Message &operator=(const Message &other) {
    if (this != &other) {
      this->~Message();
      new (this) Message(other);
    }
    return *this;
  }

  Message &operator=(Message &&other) noexcept {
    if (this != &other) {
      this->~Message();
      new (this) Message(std::move(other));
    }
    return *this;
  }

  ~Message() {
    if (header.size > inline_capacity) {
      std::free(storage.heap.ptr);
    }
  }

  Message &extend(const void *input_data, unsigned length) {
    if (header.size + length <= inline_capacity) {
      // use inline buffer
      std::memcpy(storage.buffer + header.size, input_data, length);
    } else {
      if (header.size <= inline_capacity) {
        // transition to heap once inline buffer is exhausted
        allocate_heap(std::max(header.size + length, inline_capacity * 2));
      } else if (header.size + length > storage.heap.capacity) {
        // resize if necessary
        resize_heap(header.size + length);
      }
      std::memcpy(storage.heap.ptr + header.size, input_data, length);
    }
    header.size += length;
    return *this;
  }

  Message &reserve(unsigned num_bytes) {
    if (header.size + num_bytes <= inline_capacity) {
      return *this;
    }
    if (header.size <= inline_capacity) {
      allocate_heap(header.size + num_bytes);
    } else if (header.size + num_bytes > storage.heap.capacity) {
      resize_heap(header.size + num_bytes);
    }
    return *this;
  }

  auto payload() const {
    return std::span(data(), size());
  }
  
  char const *data() const {
    return header.size <= inline_capacity
               ? static_cast<char const *>(storage.buffer)
               : static_cast<char const *>(storage.heap.ptr);
  }

  std::size_t size() const { return header.size; }

  Message &finalize() {
    // calculate crc
    return *this;
  }

  template <typename T> Message &add_field(T &&arg) {
    Reflect<std::remove_cvref_t<T>>::serialize(std::forward<T>(arg), *this);
    return *this;
  }

  Header header;

private:
  static constexpr std::uint32_t inline_capacity = 56;

  union Storage {
    char buffer[inline_capacity];
    struct HeapBuffer {
      char *ptr = nullptr;
      std::uint32_t capacity = 0;
    } heap;

    Storage() { std::memset(buffer, 0, sizeof(buffer)); }
  } storage;

  void allocate_heap(unsigned initial_capacity) {
    char *new_ptr = static_cast<char *>(std::malloc(initial_capacity));
    // assume allocating always works
    // assert(new_ptr);
    std::memcpy(new_ptr, storage.buffer, header.size);
    storage.heap = {new_ptr, initial_capacity};
  }

  void resize_heap(std::uint32_t new_capacity) {
    char *new_ptr =
        static_cast<char *>(std::realloc(storage.heap.ptr, new_capacity));
    // assume reallocating always works
    // assert(new_ptr);
    storage.heap = {new_ptr, new_capacity};
  }
};

template <std::size_t Extent = std::dynamic_extent> struct MessageParser {
  std::span<char const, Extent> message;
  unsigned cursor = 0;

  char const *current() { return message.data() + cursor; }

  bool has_n(unsigned num_bytes) {
    return cursor + num_bytes <= message.size();
  }

  template <typename T> constexpr decltype(auto) get_field() {
    return Reflect<T>::deserialize(*this);
  }
};
} // namespace rpc