#pragma once
#include <cstdint>
#include <string>

namespace slo {
struct ThreadInfo {
  std::uint64_t id;

  static ThreadInfo current() noexcept;
  void set_name(std::string const& name) const;
  [[nodiscard]] std::string get_name() const;
};

inline thread_local ThreadInfo const this_thread{ThreadInfo::current()};
}  // namespace slo