#pragma once
#include <concepts>
#include <ranges>
#include <experimental/meta>

#include <erl/util/stamp.hpp>
#include <erl/tuple.hpp>
#include <tuple>
#include <utility>

namespace erl {
struct clap;

consteval std::meta::info make_arg_tuple(std::meta::info reflection) {
  std::vector<std::meta::info> args;
  if (is_function_type(reflection)) {
    for (auto& arg : parameters_of(reflection)) {
      // turns out parameters_of of a reflection of a function type
      // returns a reflection of a type, not a reflection of a parameter
      args.push_back(arg);
    }
  } else if (is_type(reflection)) {
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
using ArgumentTuple = [:make_arg_tuple(^^T):];

namespace _default_construct_impl {
#define $generate_case(Idx)                            \
  case (Idx):                                          \
    if constexpr ((Idx) <= size) {                     \
      return visitor(std::make_index_sequence<Idx>()); \
    }                                                  \
    std::unreachable();

#define $generate_strategy(Idx, Stamper)                                         \
  template <>                                                                    \
  struct VisitStrategy<(Idx)> {                                                  \
    template <typename T, typename F>                                            \
    static T visit(F visitor, ArgumentTuple<T> const& args, std::size_t index) { \
      static constexpr auto size = std::tuple_size_v<ArgumentTuple<T>>;          \
      switch (index) { Stamper(0, $generate_case); }                             \
      std::unreachable();                                                        \
    }                                                                            \
  };
template <int>
struct VisitStrategy;
$generate_strategy(1, $stamp(4));    // 4^1 potential states
$generate_strategy(2, $stamp(16));   // 4^2 potential states
$generate_strategy(3, $stamp(64));   // 4^3 potential states
$generate_strategy(4, $stamp(256));  // 4^4 potential states

#undef $generate_strategy
#undef $generate_case

template <typename T, typename F>
T visit(F visitor, ArgumentTuple<T> const& args) {
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

  return VisitStrategy<strategy_idx>::template visit<T>(visitor, args, index);
}

}  // namespace _default_construct_impl

template <typename T>
T default_construct(ArgumentTuple<T> const& args) {
  return _default_construct_impl::visit<T>(
      [&]<std::size_t... Idx>(std::index_sequence<Idx...>) {
        if constexpr (std::derived_from<T, clap>) {
          return T{{}, *get<Idx>(args)...};
        } else {
          return T{*get<Idx>(args)...};
        }
      },
      args);
}
}  // namespace erl