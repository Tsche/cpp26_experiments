#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace slo {
namespace threading {
struct Info {
  Info() noexcept;
  explicit Info(std::uint64_t handle);

  void set_name(std::string const& name) const;
  [[nodiscard]] std::string get_name() const;
  [[nodiscard]] std::uint64_t get_id() const { return id; }

private:
  std::uint64_t id{};
};
}  // namespace threading

inline thread_local threading::Info const this_thread{};
}  // namespace slo