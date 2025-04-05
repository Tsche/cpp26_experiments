#pragma once
#include <concepts>
#include <ranges>
#include <algorithm>
#include <experimental/meta>

#include <erl/_impl/util/stamp.hpp>
#include <erl/tuple>
#include <tuple>
#include <utility>

namespace erl {
struct clap;

template <std::meta::info reflection>
consteval std::meta::info make_arg_tuple() {
  std::vector<std::meta::info> args;
  if constexpr (is_function(reflection)) {
    for (auto& arg : parameters_of(reflection)) {
      args.push_back(type_of(arg));
    }
  } else if constexpr (is_function_type(reflection)) {
    for (auto& arg : parameters_of(dealias(reflection))) {
      // turns out parameters_of of a reflection of a function type
      // returns a reflection of a type, not a reflection of a parameter
      args.push_back(arg);
    }
  } else if constexpr (is_type(reflection)) {
    for (auto& arg : nonstatic_data_members_of(reflection)) {
      args.push_back(type_of(arg));
    }
  } else {
    return {};
  }

  return substitute(^^erl::Tuple, args | std::views::transform([](auto r) {
                                    return substitute(^^std::optional, {r});
                                  }));
}

template <typename T>
using ArgumentTuple = [:make_arg_tuple<^^T>():];

namespace _default_impl {
#define $generate_case(Idx)                            \
  case (Idx):                                          \
    if constexpr ((Idx) >= Min && (Idx) <= size) {     \
      return visitor(std::make_index_sequence<Idx>()); \
    }                                                  \
    std::unreachable();

#define $generate_strategy(Idx, Stamper)                                                      \
  template <>                                                                                 \
  struct VisitStrategy<(Idx)> {                                                               \
    template <std::size_t Min, typename T, typename F>                                                         \
    static decltype(auto) visit(F visitor, ArgumentTuple<T> const& args, std::size_t index) { \
      static constexpr auto size = std::tuple_size_v<ArgumentTuple<T>>;                       \
      switch (index) { Stamper(0, $generate_case); }                                          \
      std::unreachable();                                                                     \
    }                                                                                         \
  };
template <int>
struct VisitStrategy;
$generate_strategy(1, $stamp(4));    // 4^1 potential states
$generate_strategy(2, $stamp(16));   // 4^2 potential states
$generate_strategy(3, $stamp(64));   // 4^3 potential states
$generate_strategy(4, $stamp(256));  // 4^4 potential states

#undef $generate_strategy
#undef $generate_case

template <std::size_t Min, typename T, typename F>
decltype(auto) visit(F visitor, ArgumentTuple<T> const& args) {
  constexpr static int size = std::tuple_size_v<ArgumentTuple<T>>;
  // clang-format off
  constexpr static int strategy_idx = size <= 4    ? 1
                                      : size <= 16 ? 2
                                      : size <= 64 ? 3
                                      : 4;
  // clang-format on
  auto index = [:meta::sequence(size):] >> [&]<auto... Idx> {
    std::size_t idx = 0;
    (void)((get<Idx>(args).has_value() ? ++idx, true : false) && ...);
    return idx;
  };

  return VisitStrategy<strategy_idx>::template visit<Min, T>(visitor, args, index);
}

consteval std::size_t required_args_count(std::meta::info reflection) {
  if (is_function(reflection)) {
    auto members = parameters_of(reflection);
    return std::count_if(members.begin(), members.end(), [](auto x){ return !has_default_argument(x); });
  } else if (is_type(reflection)) {
    auto members = nonstatic_data_members_of(reflection);
    return std::count_if(members.begin(), members.end(), [](auto x){ return !has_default_member_initializer(x); });
  } else {
    return {};
  }
}

}  // namespace _default_impl

template <std::meta::info T>
constexpr inline std::size_t required_arg_count = _default_impl::required_args_count(T);

template <typename T>
T default_construct(ArgumentTuple<T> const& args) {
  return _default_impl::visit<required_arg_count<dealias(^^T)>, T>(
      [&]<std::size_t... Idx>(std::index_sequence<Idx...>) {
        if constexpr (std::derived_from<T, clap>) {
          return T{{}, *get<Idx>(args)...};
        } else {
          return T{*get<Idx>(args)...};
        }
      },
      args);
}

template <std::size_t Required, typename T, typename F>
decltype(auto) default_invoke(F&& fnc, ArgumentTuple<T> const& args) {
  return _default_impl::visit<Required, T>(
      [&]<std::size_t... Idx>(std::index_sequence<Idx...>) {
        return std::forward<F>(fnc)(*get<Idx>(args)...);
      },
      args);
}
}  // namespace erl