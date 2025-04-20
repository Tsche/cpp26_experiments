#pragma once
#include <concepts>
#include <ranges>
#include <algorithm>
#include <experimental/meta>

#include <erl/_impl/util/stamp.hpp>
#include <erl/_impl/util/concepts.hpp>
#include <erl/tuple>
#include <tuple>
#include <utility>

namespace erl::_impl {
template <std::meta::info reflection>
consteval std::meta::info make_arg_tuple() {
  std::vector<std::meta::info> args;
  auto make_optional = [](auto r) { return substitute(^^std::optional, {r}); };

  if constexpr (is_function(reflection)) {
    for (auto& arg : parameters_of(reflection)) {
      args.push_back(make_optional(type_of(arg)));
    }
  } else if constexpr (is_function_type(reflection)) {
    for (auto&& arg : parameters_of(dealias(reflection))) {
      // turns out parameters_of of a reflection of a function type
      // returns a reflection of a type, not a reflection of a parameter
      args.push_back(make_optional(arg));
    }
  } else if constexpr (is_class_type(reflection)) {
    for (auto&& base : bases_of(reflection)) {
      args.push_back(extract<std::meta::info (*)()>(
          substitute(^^make_arg_tuple, {reflect_value(type_of(base))}))());
    }
    for (auto&& arg : nonstatic_data_members_of(reflection)) {
      args.push_back(make_optional(type_of(arg)));
    }
  } else {
    return {};
  }

  return substitute(^^erl::Tuple, args);
}

template <typename T>
using ArgumentTuple = [:make_arg_tuple<^^T>():];

namespace _default_impl {
#define $generate_case(Idx)                                                                        \
  case (Idx):                                                                                      \
    if constexpr ((Idx) < Max) {                                                                   \
      return visitor(std::make_index_sequence<Offset + Idx>(), std::forward<Args>(extra_args)...); \
    }                                                                                              \
    std::unreachable();

#define $generate_strategy(Idx, Stamper)                                              \
  template <>                                                                         \
  struct VisitStrategy<(Idx)> {                                                       \
    template <std::size_t Offset, std::size_t Max, typename F, typename... Args>      \
    static decltype(auto) visit(F visitor, std::size_t index, Args&&... extra_args) { \
      switch (index) { Stamper(0, $generate_case); }                                  \
      std::unreachable();                                                             \
    }                                                                                 \
  };
template <int>
struct VisitStrategy;
$generate_strategy(1, $stamp(4));  // 4^1 potential states
$generate_strategy(2, $stamp(16));   // 4^2 potential states
$generate_strategy(3, $stamp(64));   // 4^3 potential states
$generate_strategy(4, $stamp(256));  // 4^4 potential states

#undef $generate_strategy
#undef $generate_case

consteval std::size_t required_args_count(std::meta::info reflection) {
  if (is_function(reflection)) {
    auto members = parameters_of(reflection);
    return std::count_if(members.begin(), members.end(), [](auto x) {
      return !has_default_argument(x);
    });
  } else if (is_type(reflection)) {
    auto members = nonstatic_data_members_of(reflection);
    return std::count_if(members.begin(), members.end(), [](auto x) {
      return !has_default_member_initializer(x);
    });
  } else {
    return {};
  }
}

template <std::size_t Bases, std::size_t Required, typename T, typename F, typename... Args>
decltype(auto) visit(F visitor, ArgumentTuple<T> const& args, Args&&... extra_args) {
  constexpr static int size         = std::tuple_size_v<ArgumentTuple<T>>;
  constexpr static int optional_min = Bases + Required;
  constexpr static int branches     = size - optional_min;

  auto index = [:meta::sequence(size - Bases):] >> [&]<auto... Idx> {
    std::size_t idx = 0;
    (void)((get<Bases + Idx>(args).has_value() ? ++idx, true : false) && ...);
    return idx;
  };

  if (index < Required) {
    // fail
    throw 0;
  }

  if constexpr (branches == 0) {
    // no optional arguments
    return visitor(std::make_index_sequence<size - Bases>(), std::forward<Args>(extra_args)...);
  } else {
    // clang-format off
      constexpr static int strategy_idx = branches <= 4    ? 1
                                        : branches <= 16 ? 2
                                        : branches <= 64 ? 3
                                        : 4;
    // clang-format on

    return VisitStrategy<strategy_idx>::template visit<Required, branches>(
        visitor,
        index - optional_min,
        std::forward<Args>(extra_args)...);
  }
}

}  // namespace _default_impl

template <std::meta::info R>
constexpr inline std::size_t required_arg_count = _default_impl::required_args_count(R);

template <typename T>
  requires(std::is_aggregate_v<T> && !std::is_array_v<T>)
T default_construct(ArgumentTuple<T> const& args) {
  constexpr static auto base_count = 0; //meta::base_count<T>;

  return _default_impl::visit<base_count, required_arg_count<dealias(^^T)>, T>(
      [&]<std::size_t... Idx, std::size_t... BIdx>(std::index_sequence<Idx...>,
                                                   std::index_sequence<BIdx...>) {
        return T{default_construct<[:type_of(bases_of(^^T)[BIdx]):]>(get<BIdx>(args))...,
                 *get<base_count + Idx>(args)...};
      },
      args,
      std::make_index_sequence<base_count>());
}

template <std::meta::info R>
  requires (util::is_function<R> || util::is_static_member_function<R>)
decltype(auto) default_invoke(ArgumentTuple<[:type_of(R):]> const& args) {
  return _default_impl::visit<0, required_arg_count<R>, [:type_of(R):]>(
    [&]<std::size_t... Idx>(std::index_sequence<Idx...>) {
      return [:R:](*get<Idx>(args)...);
    },
    args);
}

template <std::meta::info R>
  requires (util::is_nonstatic_member_function<R>)
decltype(auto) default_invoke() = delete("calling a member function requires an object of its parent type");

template <std::meta::info R, typename T>
  requires (util::is_nonstatic_member_function<R>)
decltype(auto) default_invoke(T&& self, ArgumentTuple<[:type_of(R):]> const& args){
    static_assert(std::convertible_to<std::remove_cvref_t<T>, [:parent_of(R):]>, "wrong type for self argument");

    return _default_impl::visit<0, required_arg_count<R>, [:type_of(R):]>(
      [&]<std::size_t... Idx>(std::index_sequence<Idx...>) {
        std::forward<T>(self).[:R:](*get<Idx>(args)...);
      },
      args);
}

}  // namespace erl::_impl