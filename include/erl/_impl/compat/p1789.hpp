#pragma once
#include <utility>

namespace std {
// P1789 compat
template <size_t Idx, typename T, T... Vs>
constexpr auto get(integer_sequence<T, Vs...>) {
  return Vs...[Idx];
}
}  // namespace std

template <typename T, T... Vs>
struct std::tuple_size<std::integer_sequence<T, Vs...>>
    : std::integral_constant<size_t, sizeof...(Vs)> {};

template <std::size_t Idx, typename T, T... Vs>
struct std::tuple_element<Idx, std::integer_sequence<T, Vs...>> {
  using type = T;
};
