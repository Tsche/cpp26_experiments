#pragma once
#include <ctime>
#include <string_view>
#include <span>
#include <source_location>
#include <format>

#include <experimental/meta>

#include <erl/net/message/buffer.hpp>
#include <erl/util/string.hpp>

#include "message.hpp"
#include <erl/net/message/reader.hpp>

namespace erl::logging {
namespace impl {

template <std::size_t N>
auto unwrap(std::array<char, N> data){
  // awful hack to get string literal arguments working
  // TODO do proper safe deserialization
  return data.data();
}

template <typename T>
auto unwrap(T&& obj) {
  return std::forward<T>(obj);
}

template <typename T>
struct Replacements {
  // default - already safe
  using type = T;
};

template <typename T>
using safe_type = typename Replacements<T>::type;

template <util::fixed_string fmt_string, Location loc, typename... Args>
impl::FormattingResult format(std::span<char const> data) {
  auto fmt = std::format_string<std::remove_cvref_t<std::decay_t<Args>>&...>{fmt_string};

  auto reader  = message::MessageView{data};
  return [&](Args... args){
    // TODO do proper safe deserialization
    return impl::FormattingResult{.location = loc, .text=std::format(fmt, args...)};
  }(unwrap(deserialize<Args>(reader))...);
}

template <typename... Args>
struct FormatString {
  formatter_type format{};

  template <typename Tp>
    requires std::convertible_to<Tp const&, std::string_view>
  consteval explicit(false)
      FormatString(Tp const& str, std::source_location const location = std::source_location::current()) {
    auto fmt = util::fixed_string{str};

    auto loc = logging::Location{.file     = location.file_name(),
                                 .function = location.function_name(),
                                 .line     = location.line(),
                                 .column   = location.column()};

    format = extract<formatter_type>(
        substitute(^^logging::impl::format,
                   {std::meta::reflect_value(fmt), std::meta::reflect_value(loc), ^^std::remove_cvref_t<Args>...}));
  }
};
}  // namespace impl

template <typename... Args>
using FormatString = impl::FormatString<std::type_identity_t<Args>...>;
}  // namespace erl::logging