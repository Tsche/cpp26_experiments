#pragma once
#include <experimental/meta>

#include <concepts>
#include <stdexcept>
#include <string>
#include <format>

#include <erl/tuple.hpp>
#include <erl/_impl/util/string.hpp>
#include <erl/_impl/util/concepts.hpp>
#include <erl/_impl/util/meta.hpp>
#include <erl/_impl/util/operators.hpp>

namespace erl::_expect_impl {
template <typename T>
consteval auto consteval_wrap(T const& value) {
  // this hack is required to compare against string literals
  // in expect annotations
  if constexpr (std::convertible_to<T, std::string_view>) {
    return std::meta::define_static_string(value);
  } else {
    return value;
  }
}

template <typename T>
using wrapped_t = decltype(consteval_wrap(std::declval<T>()));

template <typename T, int rank>
concept has_higher_rank = !requires { T::rank; } || T::rank < rank;

#define $make_op(class_name, op)                                                                 \
  template <typename T>                                                                          \
  constexpr friend auto operator op(class_name rhs_, T lhs_)                                     \
    requires(has_higher_rank<T, rank>)                                                           \
  {                                                                                              \
    if consteval {                                                                               \
      return BinaryExpr<util::to_operator(#op), class_name, wrapped_t<T>>{rhs_,                  \
                                                                          consteval_wrap(lhs_)}; \
    } else {                                                                                     \
      return BinaryExpr<util::to_operator(#op), class_name, T>{rhs_, lhs_};                      \
    }                                                                                            \
  }                                                                                              \
  template <typename T>                                                                          \
  constexpr friend auto operator op(T rhs_, class_name lhs_)                                     \
    requires(has_higher_rank<T, rank * 2>)                                                       \
  {                                                                                              \
    if consteval {                                                                               \
      return BinaryExpr<util::to_operator(#op), wrapped_t<T>, class_name>{consteval_wrap(rhs_),  \
                                                                          lhs_};                 \
    } else {                                                                                     \
      return BinaryExpr<util::to_operator(#op), T, class_name>{rhs_, lhs_};                      \
    }                                                                                            \
  }

template <std::meta::operators OP, typename Rhs, typename Lhs>
struct BinaryExpr {
  static constexpr auto rank = 3;

  Rhs rhs;
  Lhs lhs;

  // comparisons
  $make_op(BinaryExpr, <);
  $make_op(BinaryExpr, <=);
  $make_op(BinaryExpr, >);
  $make_op(BinaryExpr, >=);
  $make_op(BinaryExpr, ==);
  $make_op(BinaryExpr, !=);

  // logical
  $make_op(BinaryExpr, &&);
  $make_op(BinaryExpr, ||);

  decltype(auto) eval(auto const& args) const {
    static constexpr bool rhs_evaluatable = requires {
      { rhs.eval(args) } -> util::is_non_void;
    };
    static constexpr bool lhs_evaluatable = requires {
      { lhs.eval(args) } -> util::is_non_void;
    };
    if constexpr (rhs_evaluatable && lhs_evaluatable) {
      return eval_operator(rhs.eval(args), lhs.eval(args));
    } else if constexpr (rhs_evaluatable) {
      return eval_operator(rhs.eval(args), lhs);
    } else if constexpr (lhs_evaluatable) {
      return eval_operator(rhs, lhs.eval(args));
    }
  }

  auto eval_verbose(auto const& args, std::vector<std::string>& failed_terms) const {
    static constexpr bool rhs_evaluatable = requires {
      { rhs.eval_verbose(args, failed_terms) } -> util::is_non_void;
    };
    static constexpr bool lhs_evaluatable = requires {
      { lhs.eval_verbose(args, failed_terms) } -> util::is_non_void;
    };
    if constexpr (rhs_evaluatable && lhs_evaluatable) {
      auto rhs_   = rhs.eval_verbose(args, failed_terms);
      auto lhs_   = lhs.eval_verbose(args, failed_terms);
      auto result = eval_operator(rhs_, lhs_);
      if (!result) {
        failed_terms.push_back(to_string(args));
      }
      return result;
    } else if constexpr (rhs_evaluatable) {
      auto rhs_   = rhs.eval_verbose(args, failed_terms);
      auto result = eval_operator(rhs_, lhs);
      if (!result) {
        failed_terms.push_back(to_string(args));
      }
      return result;
    } else if constexpr (lhs_evaluatable) {
      auto lhs_   = lhs.eval_verbose(args, failed_terms);
      auto result = eval_operator(rhs, lhs_);
      if (!result) {
        failed_terms.push_back(to_string(args));
      }
      return result;
    }
  }

  std::string to_string(auto const& args) const {
    // TODO constexpr
    static constexpr bool rhs_printable = requires {
      { rhs.to_string(args) } -> std::same_as<std::string>;
    };
    static constexpr bool lhs_printable = requires {
      { lhs.to_string(args) } -> std::same_as<std::string>;
    };

    if constexpr (rhs_printable && lhs_printable) {
      return std::format("({} {} {})", rhs.to_string(args), util::to_string(OP), lhs.to_string(args));
    } else if constexpr (rhs_printable) {
      return std::format("({} {} {})", rhs.to_string(args), util::to_string(OP), lhs);
    } else if constexpr (lhs_printable) {
      return std::format("({} {} {})", rhs, util::to_string(OP), lhs.to_string(args));
    }
  }

  constexpr std::string to_string(std::vector<std::string_view> const& replacements) const {
    static constexpr bool rhs_printable = requires {
      { rhs.to_string(replacements) } -> std::same_as<std::string>;
    };
    static constexpr bool lhs_printable = requires {
      { lhs.to_string(replacements) } -> std::same_as<std::string>;
    };

    if constexpr (rhs_printable && lhs_printable) {
      return std::string("(") + rhs.to_string(replacements) + " " + util::to_string(OP) + " " + lhs.to_string(replacements) + ")";
    } else if constexpr (rhs_printable) {
      return std::string("(") + rhs.to_string(replacements) + " " + util::to_string(OP) + " " + util::to_string(lhs) + ")";
    } else if constexpr (lhs_printable) {
      return std::string("(") + util::to_string(rhs) + " " + util::to_string(OP) + " " + lhs.to_string(replacements) + ")";
    }
  }

  static constexpr decltype(auto) eval_operator(auto lhs, auto rhs) {
    using enum std::meta::operators;
    /* */ if constexpr (OP == op_plus) {
      return lhs + rhs;
    } else if constexpr (OP == op_minus) {
      return lhs - rhs;
    } else if constexpr (OP == op_star) {
      return lhs * rhs;
    } else if constexpr (OP == op_slash) {
      return lhs / rhs;
    } else if constexpr (OP == op_percent) {
      return lhs % rhs;
    } else if constexpr (OP == op_caret) {
      return lhs ^ rhs;
    } else if constexpr (OP == op_ampersand) {
      return lhs & rhs;
    } else if constexpr (OP == op_pipe) {
      return lhs | rhs;
    } else if constexpr (OP == op_equals) {
      return lhs = rhs;
    } else if constexpr (OP == op_plus_equals) {
      return lhs += rhs;
    } else if constexpr (OP == op_minus_equals) {
      return lhs -= rhs;
    } else if constexpr (OP == op_star_equals) {
      return lhs *= rhs;
    } else if constexpr (OP == op_slash_equals) {
      return lhs /= rhs;
    } else if constexpr (OP == op_percent_equals) {
      return lhs %= rhs;
    } else if constexpr (OP == op_caret_equals) {
      return lhs ^= rhs;
    } else if constexpr (OP == op_ampersand_equals) {
      return lhs &= rhs;
    } else if constexpr (OP == op_pipe_equals) {
      return lhs |= rhs;
    } else if constexpr (OP == op_equals_equals) {
      return lhs == rhs;
    } else if constexpr (OP == op_exclamation_equals) {
      return lhs != rhs;
    } else if constexpr (OP == op_less) {
      return lhs < rhs;
    } else if constexpr (OP == op_greater) {
      return lhs > rhs;
    } else if constexpr (OP == op_less_equals) {
      return lhs <= rhs;
    } else if constexpr (OP == op_greater_equals) {
      return lhs >= rhs;
    } else if constexpr (OP == op_spaceship) {
      return lhs <=> rhs;
    } else if constexpr (OP == op_ampersand_ampersand) {
      return lhs && rhs;
    } else if constexpr (OP == op_pipe_pipe) {
      return lhs || rhs;
    } else if constexpr (OP == op_less_less) {
      return lhs << rhs;
    } else if constexpr (OP == op_greater_greater) {
      return lhs >> rhs;
    } else if constexpr (OP == op_less_less_equals) {
      return lhs <<= rhs;
    } else if constexpr (OP == op_greater_greater_equals) {
      return lhs >>= rhs;
    } else if constexpr (OP == op_comma) {
      return lhs, rhs;
    }
  }
};

template <std::size_t Idx>
struct Placeholder {
  static constexpr auto rank = 1;

  // comparisons
  $make_op(Placeholder, <);
  $make_op(Placeholder, <=);
  $make_op(Placeholder, >);
  $make_op(Placeholder, >=);
  $make_op(Placeholder, ==);
  $make_op(Placeholder, !=);

  // logical
  $make_op(Placeholder, &&);
  $make_op(Placeholder, ||);

  decltype(auto) eval(auto const& args) const {
    static constexpr auto placeholder_count =
        std::tuple_size_v<std::remove_cvref_t<decltype(args)>>;
    static_assert(Idx < placeholder_count,
                  std::string("Placeholder $") + util::utos(Idx) + " has no associated value");
    return get<Idx>(args);
  }

  auto eval_verbose(auto const& args, std::vector<std::string>& failed_terms) const {
    return eval(args);
  }

  std::string to_string(auto const& args) const { 
    // TODO constexpr
    return std::format("{}", get<Idx>(args));
  }

  constexpr std::string to_string(std::vector<std::string_view> const& replacements) const {
    return std::string(replacements[Idx]);
  }
};

template <typename F, typename... Args>
struct Lazy {
  static constexpr auto rank = 2;

  F callable;
  std::tuple<Args...> arguments;

  // comparisons
  $make_op(Lazy, <);
  $make_op(Lazy, <=);
  $make_op(Lazy, >);
  $make_op(Lazy, >=);
  $make_op(Lazy, ==);
  $make_op(Lazy, !=);

  // logical
  $make_op(Lazy, &&);
  $make_op(Lazy, ||);

  template <std::size_t Idx, bool verbose = false>
  decltype(auto) expand_args(auto const& args, std::vector<std::string>& failed_terms) const {
    using wrapped_t = Args...[Idx];
    if constexpr (verbose && requires(wrapped_t obj) { obj.eval_verbose(args, failed_terms); }) {
      return get<Idx>(arguments).eval_verbose(args, failed_terms);
    } else if constexpr (requires(wrapped_t obj) { obj.eval(args); }) {
      return get<Idx>(arguments).eval(args);
    } else {
      return get<Idx>(arguments);
    }
  }

  decltype(auto) eval(auto const& args) const {
    return [:meta::sequence(sizeof...(Args)):] >> [&]<auto... Idx> {
      return callable(expand_args<Idx>(args)...);
    };
  }

  auto eval_verbose(auto const& args, std::vector<std::string>& failed_terms) const {
    return eval(args);
  }

  constexpr std::string to_string(auto const& args) const { return "<lazy call>"; }
  constexpr std::string to_string(std::vector<std::string_view> const&) const { return "<lazy call>"; }
};

#define LAZY_ARGS(...)    \
  erl::Tuple(__VA_ARGS__) \
  }
#define $(fnc) \
  Lazy {       \
    []<typename... Ts>(Ts&&... args) { return fnc(std::forward<Ts>(args)...); }, LAZY_ARGS

struct LazyProxy {
  template <typename Fn>
  constexpr auto operator()(Fn fn) const {
    return [fn](auto&&... args) { return Lazy{fn, erl::Tuple(args...)}; };
  }
};
}  // namespace erl::_expect_impl