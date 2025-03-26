#pragma once
#include <functional>
#include <vector>
#include <ranges>

#include <experimental/meta>

#include "string.hpp"

namespace erl::meta {
consteval auto member_functions_of(std::meta::info reflection) {
  return members_of(reflection) | std::views::filter(std::meta::is_public) |
         std::views::filter(std::meta::is_function) |
         std::views::filter(std::not_fn(std::meta::is_special_member_function)) |
         std::views::filter(std::not_fn(std::meta::is_conversion_function)) |
         std::views::filter(std::not_fn(std::meta::is_operator_function)) |
         std::views::filter(std::not_fn(std::meta::is_static_member)) | std::ranges::to<std::vector>();
}

consteval auto named_members_of(std::meta::info reflection) {
  return members_of(reflection) | std::views::filter(std::meta::has_identifier) | std::ranges::to<std::vector>();
}

consteval auto get_annotation(std::meta::info item, std::meta::info type) {
  if (is_type(type)) {
    return annotations_of(item, type)[0];
  } else if (is_template(type)) {
    for (auto annotation : annotations_of(item)) {
      if (has_template_arguments(type_of(annotation)) && template_of(type_of(annotation)) == type) {
        return annotation;
      }
    }
  }
  return ^^::;
}

template <typename T>
consteval bool has_annotation(std::meta::info item) {
  return !annotations_of(item, ^^T).empty();
}

consteval bool has_annotation(std::meta::info item, std::meta::info type) {
  if (is_type(type)) {
    return !annotations_of(item, type).empty();
  } else if (is_template(type)) {
    for (auto annotation : annotations_of(item)) {
      if (has_template_arguments(type_of(annotation)) && template_of(type_of(annotation)) == type) {
        return true;
      }
    }
  }
  return false;
}

template <typename T>
consteval bool has_annotation(std::meta::info item, T const& value) {
  if (!has_annotation<T>(item)) {
    return false;
  }
  auto annotations = annotations_of(item, ^^T);
  auto value_r     = std::meta::reflect_value(value);
  return std::ranges::any_of(annotations, [&](auto annotation) { return annotation == value_r; });
}

template <typename T>
consteval auto nth_nsdm(std::size_t index) {
  return nonstatic_data_members_of(^^T)[index];
}

consteval std::meta::info get_nth_member(std::meta::info reflection, std::size_t n) {
  return nonstatic_data_members_of(reflection)[n];
}

template <typename T>
consteval std::size_t get_member_index(std::string_view name) {
  std::vector<std::string_view> names =
      nonstatic_data_members_of(^^T) | std::views::transform(std::meta::identifier_of) | std::ranges::to<std::vector>();
  if (auto it = std::ranges::find(names, name); it != names.end()) {
    return std::distance(names.begin(), it);
  }
  return -1UZ;
}

template <typename T>
consteval bool has_member(std::string_view name) {
  return get_member_index<T>(name) != -1ULL;
}

template <typename T>
consteval std::vector<std::string_view> get_member_names() {
  return nonstatic_data_members_of(^^T) | std::views::transform(std::meta::identifier_of) |
         std::ranges::to<std::vector>();
}

template <typename T>
constexpr inline std::size_t member_count = nonstatic_data_members_of(^^T).size();

template <char... Vs>
constexpr inline auto static_string = util::fixed_string<sizeof...(Vs)>{Vs...};

consteval auto promote(std::string_view str) {
  std::vector<std::meta::info> args;
  for (auto character : str) {
    args.push_back(std::meta::reflect_value(character));
  }
  return substitute(^^static_string, args);
}

template <typename T, T... Vs>
constexpr inline T fixed_array[sizeof...(Vs)]{Vs...};

template <std::ranges::input_range R>
  requires(!std::same_as<std::ranges::range_value_t<R>, char>)
consteval auto promote(R&& iterable) {
  std::vector args = {^^std::ranges::range_value_t<R>};
  for (auto element : iterable) {
    args.push_back(std::meta::reflect_value(element));
  }
  return substitute(^^fixed_array, args);
}

namespace impl {
template <auto... Elts>
struct Replicator {
  template <typename F>
  constexpr decltype(auto) operator>>(F fnc) const {
    return fnc.template operator()<Elts...>();
  }

  template <typename F>
  constexpr void operator>>=(F fnc) const {
    (fnc.template operator()<Elts>(), ...);
  }
};

template <auto... Elts>
constexpr inline Replicator<Elts...> replicator{};

template <std::size_t Idx, auto... Elts>
constexpr auto get(Replicator<Elts...> const&){
    return Elts...[Idx];
}
}  // namespace impl

template <std::ranges::range R>
consteval auto expand(R const& range) {
  std::vector<std::meta::info> args;
  for (auto item : range) {
    args.push_back(std::meta::reflect_value(item));
  }
  return substitute(^^impl::replicator, args);
}

template <std::ranges::range R>
consteval auto enumerate(R range) {
  std::vector<std::meta::info> args;

  // could also use std::views::enumerate(range)
  for (auto idx = 0; auto item : range) {
    args.push_back(std::meta::reflect_value(std::pair{idx++, item}));
  }
  return substitute(^^impl::replicator, args);
}

consteval auto sequence(unsigned maximum) {
  return expand(std::ranges::iota_view{0U, maximum});
}

template <typename T>
constexpr inline auto enumerator_names = [:expand(enumerators_of(^^T)):] >> []<std::meta::info... Enumerators> {
  return std::array{std::string_view{identifier_of(Enumerators)}...};
};
}  // namespace erl::meta

template <auto... Elts>
struct std::tuple_size<erl::meta::impl::Replicator<Elts...>>
    : std::integral_constant<std::size_t, sizeof...(Elts)> {};

template <std::size_t Idx, auto... Elts>
struct std::tuple_element<Idx, erl::meta::impl::Replicator<Elts...>> {
    using type = decltype(Elts...[Idx]);
};