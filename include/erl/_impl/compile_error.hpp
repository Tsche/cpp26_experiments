#pragma once

#include <string_view>
#include <experimental/meta>

namespace erl {
namespace _compile_error_impl {
template <char const* message>
consteval void error() {
  static_assert(false, std::string_view{message});
}
}  // namespace _compile_error_impl

[[noreturn]] consteval void compile_error(std::string_view message) {
  using std::meta::reflect_value;
  extract<void (*)()>(
      substitute(^^_compile_error_impl::error, {reflect_value(define_static_string(message))}))();
  std::unreachable();
}
}  // namespace erl