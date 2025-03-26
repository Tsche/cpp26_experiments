#pragma once
#include <utility>
#include <tuple>
#include <type_traits>
#include <concepts>

namespace erl::util {
template <typename T>
concept is_subscriptable = requires(T obj) { obj[0]; };

template <typename T>
using subscript_result = decltype(std::declval<T>()[0]);

template <typename T>
concept is_tuple_like = requires(T obj) {
  typename std::tuple_element_t<0, std::remove_cvref_t<T>>;
  { get<0>(obj) } -> std::convertible_to<std::tuple_element_t<0, std::remove_cvref_t<T>>>;
};

}  // namespace erl::util