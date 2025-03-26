#pragma once
#include <type_traits>
#include <cstdio>
#include <source_location>
#include <string_view>

#include <erl/util/meta.hpp>

#include "format/named.hpp"
namespace erl {

template <typename... Args>
struct NamedFormatString {
  using format_type = std::string (*)(Args&&...);
  format_type handle;

  template <typename Tp>
    requires std::convertible_to<Tp const&, std::string_view>
  consteval explicit(false)
      NamedFormatString(Tp const& str,
                        std::source_location const& location = std::source_location::current()) {
    auto [fmt, direct_list, accessors] =
        impl::_format_impl::FormatParser::parse<std::remove_cvref_t<Args>...>(str, location);

    handle = extract<format_type>(
        substitute(^^impl::_format_impl::do_format, {meta::promote(fmt), direct_list, accessors, ^^Args...}));
  }
};

template <typename... Args>
auto format(NamedFormatString<std::type_identity_t<Args>...> fmt, Args&&... args) {
  return fmt.handle(std::forward<Args>(args)...);
}

template <typename... Args>
void println(NamedFormatString<std::type_identity_t<Args>...> fmt, Args&&... args) {
  std::puts(fmt.handle(std::forward<Args>(args)...).c_str());
}
}  // namespace erl