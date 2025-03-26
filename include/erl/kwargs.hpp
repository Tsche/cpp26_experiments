#include <concepts>
#include <string_view>
#include <vector>
#include <tuple>
#include <type_traits>
#include <utility>
#include <ranges>
#include <cstddef>
#include <cstdio>
#include <string>

#include <format>

#include <experimental/meta>

#include "util/string.hpp"
#include "util/meta.hpp"
#include "util/parser.hpp"
namespace erl {

template <typename Impl>
struct [[nodiscard]] kwargs_t : Impl {
  using type = Impl;
};

template <typename T>
concept is_kwargs = has_template_arguments(^^T) && template_of(^^T) == ^^kwargs_t;

namespace kwargs {
struct NameParser : util::Parser {
  std::vector<std::string_view> names;

  constexpr bool parse() {
    cursor = 0;

    while (is_valid()) {
      skip_whitespace();

      if (current() == '&') {
        // might be captured by reference
        ++cursor;
        skip_whitespace();
      }

      if (current() == '.') {
        // pack captured, reject
        return false;
      }

      std::size_t start = cursor;

      // find '=', ',' or whitespace
      skip_to('=', ',', ' ', '\n', '\r', '\t');
      if (cursor - start == 0) {
        // default capture or invalid name
        return false;
      }

      auto name = data.substr(start, cursor - start);
      if (name == "this" || name == "*this") {
        // this captured, reject
        return false;
      }
      names.emplace_back(name);

      // skip ahead to next capture
      // if the current character is already ',', this will not move the cursor
      skip_to(',');
      ++cursor;

      skip_whitespace();
    }
    return true;
  }
};

template <util::fixed_string Names, typename... Ts>
constexpr auto make(Ts&&... values) {
  struct kwargs_impl;
  consteval {
    std::vector<std::meta::info> types{^^Ts...};
    std::vector<std::meta::info> args;

    auto parser = NameParser{Names};

    // with P3068 parser.parse() could throw to provide better diagnostics at this point
    if (!parser.parse()) {
      return;
    }

    // associate every argument with the corresponding name
    // retrieved by parsing the capture list

    // std::views::zip_transform could also be used for this
    for (auto [member, name] : std::views::zip(types, parser.names)) {
      args.push_back(data_member_spec(member, {.name = name}));
    }
    define_aggregate(^^kwargs_impl, args);
  };

  // ensure injecting the class worked
  static_assert(is_type(^^kwargs_impl), std::string{"Invalid keyword arguments `"} + Names + "`");

  return kwargs_t<kwargs_impl>{{std::forward<Ts>(values)...}};
}

template <util::fixed_string Names, typename T>
auto from_lambda(T&& lambda) {
  using fnc_t = std::remove_cvref_t<T>;

  return [:meta::expand(nonstatic_data_members_of(^^fnc_t)):] >> [&]<auto... member>() {
    return make<Names>(std::forward<T>(lambda).[:member:]...);
  };
}
}  // namespace kwargs

template <util::fixed_string Names, typename... Ts>
constexpr auto make_args(Ts&&... values) {
  return kwargs::make<Names>(std::forward<Ts>(values)...);
}

template <typename T>
consteval bool has_arg(std::string_view name) {
  if constexpr (is_kwargs<std::remove_cvref_t<T>>) {
    return meta::has_member<typename std::remove_cvref_t<T>::type>(name);
  } else {
    return meta::has_member<T>(name);
  }
}

// get

template <std::size_t I, typename T>
  requires is_kwargs<std::remove_cvref_t<T>>
constexpr auto get(T&& kwargs) noexcept {
  using kwarg_tuple = typename std::remove_cvref_t<T>::type;
  static_assert(meta::member_count<kwarg_tuple> > I);

  return std::forward<T>(kwargs).[:meta::get_nth_member(^^kwarg_tuple, I):];
}

template <util::fixed_string name, typename T>
  requires is_kwargs<std::remove_cvref_t<T>>
constexpr auto get(T&& kwargs) {
  using kwarg_tuple = typename std::remove_cvref_t<T>::type;
  static_assert(meta::has_member<kwarg_tuple>(name), "Keyword argument `" + std::string(name) + "` not found.");

  return std::forward<T>(kwargs).[:meta::get_nth_member(^^kwarg_tuple, meta::get_member_index<kwarg_tuple>(name)):];
}

// get_or
template <std::size_t I, typename T, typename R>
  requires is_kwargs<std::remove_cvref_t<T>>
constexpr auto get_or(T&& kwargs, R default_) noexcept {
  using kwarg_tuple = typename std::remove_cvref_t<T>::type;
  if constexpr (meta::member_count<kwarg_tuple> > I) {
    return get<I>(std::forward<T>(kwargs));
  } else {
    return default_;
  }
}

template <util::fixed_string name, typename T, typename R>
  requires is_kwargs<std::remove_cvref_t<T>>
constexpr auto get_or(T&& kwargs, R default_) {
  using kwarg_tuple = typename std::remove_cvref_t<T>::type;
  if constexpr (meta::member_count<kwarg_tuple> > meta::get_member_index<kwarg_tuple>(name)) {
    return get<name>(std::forward<T>(kwargs));
  } else {
    return default_;
  }
}

namespace formatting {
struct FmtParser : util::Parser {
  constexpr std::string transform(std::ranges::input_range auto const& names) {
    std::string out;
    while (is_valid()) {
      out += current();
      if (current() == '{') {
        ++cursor;
        if (current() == '{') {
          // double curly braces means escaped curly braces
          // => treat the content as text
          auto start = cursor;
          // skip to first unbalanced }
          // this will match the outer {
          skip_to('}');
          out += data.substr(start, cursor - start);
          continue;
        }

        // find name
        auto start = cursor;
        skip_to('}', ':');
        auto name = data.substr(start, cursor - start);

        // replace name
        auto it  = std::ranges::find(names.begin(), names.end(), name);
        auto idx = std::distance(names.begin(), it);
        out += util::utos(idx);

        out += current();
      }
      ++cursor;
    }
    return out;
  }
};

template <util::fixed_string fmt, typename Args>
std::string format_impl(Args const& kwargs) {
  return [:meta::sequence(std::tuple_size_v<Args>):] >> [&]<std::size_t... Idx>() {
    return std::format(fmt, get<Idx>(kwargs)...);
  };
}

template <typename Args>
struct NamedFormatString {
  using format_type = std::string (*)(Args const&);
  format_type format;

  template <typename Tp>
    requires std::convertible_to<Tp const&, std::string_view>
  consteval explicit(false) NamedFormatString(Tp const& str) {
    auto parser = FmtParser{str};
    auto fmt    = parser.transform(meta::get_member_names<typename Args::type>());
    format      = extract<format_type>(substitute(^^format_impl, {meta::promote(fmt), ^^Args}));
  }
};
}  // namespace formatting

template <typename T>
using named_format_string = formatting::NamedFormatString<std::type_identity_t<T>>;

// template <typename T>
//   requires(is_kwargs<T>)
// void print(erl::named_format_string<T> fmt, T const& kwargs) {
//   fputs(fmt.format(kwargs).c_str(), stdout);
// }

// template <typename... Args>
//   requires(sizeof...(Args) != 1 || (!is_kwargs<std::remove_cvref_t<Args>> && ...))
// void print(std::format_string<Args...> fmt, Args&&... args) {
//   std::print(fmt, std::forward<Args>(args)...);
// }

// template <typename T>
//   requires(is_kwargs<T>)
// void println(erl::named_format_string<T> fmt, T const& kwargs) {
//   puts(fmt.format(kwargs).c_str());
// }

// template <typename... Args>
//   requires(sizeof...(Args) != 1 || (!is_kwargs<std::remove_cvref_t<Args>> && ...))
// void println(std::format_string<Args...> fmt, Args&&... args) {
//   std::println(fmt, std::forward<Args>(args)...);
// }

// template <typename T>
//   requires(is_kwargs<T>)
// std::string format(erl::named_format_string<T> fmt, T const& kwargs) {
//   return fmt.format(kwargs);
// }

// template <typename... Args>
//   requires(sizeof...(Args) != 1 || (!is_kwargs<std::remove_cvref_t<Args>> && ...))
// std::string format(std::format_string<Args...> fmt, Args&&... args) {
//   return std::format(fmt, std::forward<Args>(args)...);
// }

#if __has_feature(parameter_reflection)
namespace kwargs {
template <std::meta::info F>
  requires(is_function(F))
struct Wrap {
  template <typename T, std::size_t PosOnly = 0>
  static constexpr void check_args() {
    [:erl::meta::expand(parameters_of(F) | std::views::take(PosOnly)):] >>= [&]<auto Param> {
      static_assert(!erl::meta::has_member<T>(identifier_of(Param)),
                    "In call to `" + std::string(identifier_of(F)) + "`: Positional argument `" + identifier_of(Param) +
                        "` repeated as keyword argument.");
    };

    [:erl::meta::expand(parameters_of(F) | std::views::drop(PosOnly)):] >>= [&]<auto Param> {
      static_assert(
          erl::meta::has_member<T>(identifier_of(Param)),
          "In call to `" + std::string(identifier_of(F)) + "`: Argument `" + identifier_of(Param) + "` missing.");
    };
  }

  static constexpr decltype(auto) operator()()
    requires requires { [:F:](); }
  {
    return [:F:]();
  }

  template <typename... Args>
    requires(sizeof...(Args) > 0)
  static constexpr decltype(auto) operator()(Args&&... args) {
    static constexpr std::size_t args_size = sizeof...(Args) - 1;
    using T                                = std::remove_cvref_t<Args...[args_size]>;

    if constexpr (erl::is_kwargs<T>) {
      check_args<typename T::type, args_size>();

      return [:erl::meta::expand(parameters_of(F) | std::views::drop(args_size)):] >> [&]<auto... Params> {
        return [:erl::meta::sequence(args_size):] >> [&]<std::size_t... Idx> {
          return [:F:](
              /* positional arguments */
              std::forward<Args...[Idx]>(args...[Idx])...,
              /* keyword arguments */
              get<erl::meta::get_member_index<typename T::type>(identifier_of(Params))>(args...[args_size])...);
        };
      };
    } else {
      // no keyword arguments
      return [:F:](std::forward<Args>(args)...);
    }
  }
};

template <auto F>
constexpr inline Wrap<F> invoke{};
}  // namespace kwargs
#endif

}  // namespace erl

template <typename T>
struct std::tuple_size<erl::kwargs_t<T>>
    : public integral_constant<size_t, erl::meta::member_count<std::remove_cvref_t<T>>> {};

template <std::size_t I, typename T>
struct std::tuple_element<I, erl::kwargs_t<T>> {
  using type = [:erl::meta::get_nth_member(^^T, I):];
};

#define $make_args(...) ::erl::kwargs::from_lambda<#__VA_ARGS__>([__VA_ARGS__] {})