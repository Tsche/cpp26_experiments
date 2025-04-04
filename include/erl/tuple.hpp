#pragma once
#include <array>
#include <experimental/meta>
#include "util/meta.hpp"

namespace erl {
template <typename... Ts>
struct Tuple {
  struct Storage;

  consteval {
    define_aggregate(^^Storage, {data_member_spec(^^Ts)...});
  }
  Storage data;

  Tuple() : data{} {}
  Tuple(Ts const&... values) requires(sizeof...(Ts) > 0) : data{values...} {}
};

template <std::size_t I, typename... Ts>
constexpr auto get(Tuple<Ts...>& t) noexcept -> std::tuple_element_t<I, Tuple<Ts...>>& {
  return t.data.[:meta::nth_nsdm<decltype(t.data)>(I):];
}

template <std::size_t I, typename... Ts>
constexpr auto get(Tuple<Ts...> const& t) noexcept -> std::tuple_element_t<I, Tuple<Ts...>> const& {
  return t.data.[:meta::nth_nsdm<decltype(t.data)>(I):];
}

template <std::size_t I, typename... Ts>
constexpr auto get(Tuple<Ts...>&& t) noexcept -> std::tuple_element_t<I, Tuple<Ts...>>&& {
  return std::move(t).data.[:meta::nth_nsdm<decltype(t.data)>(I):];
}
}  // namespace erl

template <typename... Ts>
struct std::tuple_size<erl::Tuple<Ts...>> : public integral_constant<size_t, sizeof...(Ts)> {};

template <std::size_t I, typename... Ts>
struct std::tuple_element<I, erl::Tuple<Ts...>> {
  static constexpr std::array types = {^^Ts...};
  using type                        = [:types[I]:];
};