#pragma once
#include <cstddef>
#include <string>

namespace erl::thread::impl {
  void set_name(std::uint64_t id, std::string const& name);
  std::string get_name(std::uint64_t id);
  std::uint64_t current() noexcept;
}