#pragma once
#include <experimental/meta>
#include <functional>
#include <vector>
#include <ranges>


namespace rpc::meta {
consteval auto member_functions_of(std::meta::info reflection) {
  return members_of(reflection) | std::views::filter(std::meta::is_public) |
         std::views::filter(std::meta::is_function) |
         std::views::filter(
             std::not_fn(std::meta::is_special_member_function)) |
         std::views::filter(std::not_fn(std::meta::is_conversion_function)) |
         std::views::filter(std::not_fn(std::meta::is_operator_function)) |
         std::views::filter(std::not_fn(std::meta::is_static_member)) |
         std::ranges::to<std::vector>();
}

template <typename T>
consteval auto nth_nsdm(std::size_t index){
  return nonstatic_data_members_of(^T)[index];
}

namespace impl{
  template <auto... Vs>
  struct Replicator {
    template <typename F>
    constexpr decltype(auto) operator>>(F fnc) const {
      return fnc.template operator()<Vs...>();
    }
  };

  template <auto... Vs>
  constexpr static Replicator<Vs...> replicator{};
}

template <std::ranges::range R>
consteval auto expand(R range){
  std::vector<std::meta::info> args;
  for (auto item : range){
    args.push_back(reflect_value(item));
  }
  return substitute(^impl::replicator, args);
}
} // namespace meta