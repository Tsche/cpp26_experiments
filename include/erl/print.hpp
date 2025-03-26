#pragma once
#include <type_traits>
#include <cstdio>

#include "format/named.hpp"
namespace erl {
  template <typename... Args>
auto format(formatting::NamedFormatString<std::type_identity_t<Args>...> fmt, Args&&... args) {
  return fmt.handle(std::forward<Args>(args)...);
}

template <typename... Args>
void println(formatting::NamedFormatString<std::type_identity_t<Args>...> fmt, Args&&... args) {
  std::puts(fmt.handle(std::forward<Args>(args)...).c_str());
}
}