#pragma once
#include <cstddef>
#include <utility>
#include <type_traits>

namespace erl::util {
template <typename... Ts>
struct TypeList {
  static constexpr auto size = sizeof...(Ts);
};

template <auto... Vs>
struct ValueList {
  static constexpr auto size = sizeof...(Vs);
};

template <std::size_t Idx, typename... Ts>
constexpr auto get(TypeList<Ts...>) {
  return std::type_identity<Ts... [Idx]> {};
}

template <std::size_t Idx, auto... Vs>
constexpr auto get(ValueList<Vs...>) {
  return Vs...[Idx];
}
}  // namespace erl::util

template <typename... Ts>
struct std::tuple_size<erl::util::TypeList<Ts...>>
    : std::integral_constant<size_t, sizeof...(Ts)> {};

template <auto... Vs>
struct std::tuple_size<erl::util::ValueList<Vs...>>
    : std::integral_constant<size_t, sizeof...(Vs)> {};

template <std::size_t Idx, typename... Ts>
struct std::tuple_element<Idx, erl::util::TypeList<Ts...>> {
  using type = Ts...[Idx];
};

template <std::size_t Idx, auto... Vs>
struct std::tuple_element<Idx, erl::util::ValueList<Vs...>> {
  using type = decltype(Vs...[Idx]);
};
