#pragma once
#include <type_traits>

namespace util {
template <typename T>
concept pair_like = requires(T obj) {
  obj.first;
  obj.second;
  requires std::is_same_v<typename T::first_type, decltype(obj.first)>;
  requires std::is_same_v<typename T::second_type, decltype(obj.second)>;
};

}