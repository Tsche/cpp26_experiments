#pragma once
#include <atomic>
#include <cstddef>
#include <new>

#include "base.hpp"


namespace slo::queues {
template <typename T, std::size_t N>
struct BoundedSPSC : impl::QueueBase {
  static_assert(N >= 2, "Must be able to store at least 2 elements.");
  static_assert((N & (N - 1)) == 0, "Maximum number of elements must be power of 2.");
  static constexpr auto buffer_mask    = N - 1;
  constexpr static auto cacheline_size = std::hardware_destructive_interference_size;
  static constexpr auto capacity       = N;
  using element_type                   = T;
  using cell_type                      = T;

  bool try_push(T const& data) {
    auto pos  = write_pos.load(std::memory_order_relaxed);
    auto next = (pos + 1) & buffer_mask;
    if (next == read_pos_cached) {
      read_pos_cached = read_pos.load(std::memory_order_acquire);
      if (next == read_pos_cached) {
        return false;
      }
    }

    new (buffer + pos) T(data);
    write_pos.store(next, std::memory_order_release);
    return true;
  }

  bool try_pop(T* target) {
    auto pos = read_pos.load(std::memory_order_relaxed);
    if (pos == write_pos_cached) {
      write_pos_cached = write_pos.load(std::memory_order_acquire);
      if (pos == write_pos_cached) {
        return false;
      }
    }

    new (target) T(buffer[pos]);
    auto next = (pos + 1) & buffer_mask;
    read_pos.store(next, std::memory_order_release);
    return true;
  }

  bool is_empty() const {
    return write_pos.load(std::memory_order_relaxed) == read_pos.load(std::memory_order_relaxed);
  }

private:
  T buffer[N];

  alignas(cacheline_size) std::atomic<std::size_t> write_pos;
  size_t write_pos_cached{0};
  alignas(cacheline_size) std::atomic<std::size_t> read_pos;
  size_t read_pos_cached{0};
};
}  // namespace slo::queues