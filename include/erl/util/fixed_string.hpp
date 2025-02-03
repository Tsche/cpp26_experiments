#pragma once
#include <cstddef>
#include <string_view>

namespace erl::util {

template <std::size_t N>
struct fixed_string {
  constexpr static auto size = N;

  constexpr fixed_string() = default;
  constexpr explicit(false) fixed_string(const char (&str)[N + 1]) noexcept {
    auto idx = std::size_t{0};
    for (char const chr : str) {
      data[idx++] = chr;
    }
  }

  [[nodiscard]] constexpr std::string_view to_sv() const { return std::string_view{data}; }

  char data[N + 1]{};
};

template <std::size_t N>
fixed_string(char const (&)[N]) -> fixed_string<N - 1>;
}  // namespace erl::util