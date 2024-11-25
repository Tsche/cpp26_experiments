#pragma once
#include <type_traits>

namespace util {
template <typename T>
concept pair_like = requires(T t) {
  t.first;
  t.second;
  requires std::is_same_v<typename T::first_type, decltype(t.first)>;
  requires std::is_same_v<typename T::second_type, decltype(t.second)>;
};

}