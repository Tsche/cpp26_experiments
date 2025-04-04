#pragma once
#include <cstddef>
#include <string_view>
#include <string>
#include <concepts>
#include <type_traits>

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
  constexpr explicit fixed_string(std::same_as<char> auto... Vs) : data{Vs...} {}
  constexpr explicit fixed_string(std::string_view str) { str.copy(data, N); }
  constexpr explicit(false) operator std::string_view() const { return std::string_view{data}; }

  char data[N + 1]{};
};

template <std::size_t N>
fixed_string(char const (&)[N]) -> fixed_string<N - 1>;

constexpr std::string utos(std::uint64_t value) {
  std::string out{};
  do {
    out += static_cast<char>('0' + (value % 10U));
    value /= 10;
  } while (value > 0);
  return {out.rbegin(), out.rend()};
}

constexpr std::uint64_t stou(std::string_view str) {
    unsigned result = 0;
    for (char const c : str) {
        (result *= 10) += c - '0';
    }
    return result;
}

constexpr std::string to_string(bool value){
  return value ? "true" : "false";
}

constexpr std::string to_string(std::convertible_to<std::string_view> auto value){
  return std::string{value};
}

constexpr std::string to_string(std::integral auto value){
  if constexpr (std::is_signed_v<decltype(value)>){
    if (value < 0){
      return std::string{'-'} + utos(-value);
    }
  }
  return utos(value);
}

}  // namespace erl::util