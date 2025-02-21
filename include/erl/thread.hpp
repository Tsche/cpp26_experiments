#pragma once
#include <cstdint>
#include <string>

namespace erl {
struct ThreadInfo {
  std::uint64_t id;

  [[nodiscard]] bool is_valid() const;

  static ThreadInfo current() noexcept;

  ThreadInfo const& set_name(std::string const& name) const;
  [[nodiscard]] std::string get_name() const;

  ThreadInfo const& set_parent(ThreadInfo parent) const;
  [[nodiscard]] ThreadInfo get_parent() const;
};

inline thread_local ThreadInfo const this_thread{ThreadInfo::current()};
}  // namespace erl