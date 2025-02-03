#pragma once
#include <ctime>
#include <string_view>
#include <span>
#include <source_location>
#include <format>

#include <experimental/meta>

#include <erl/net/message/buffer.hpp>
#include <erl/util/fixed_string.hpp>

#include "message.hpp"
#include <erl/net/message/reader.hpp>

namespace erl::logging {
using formatter_type = Message (*)(std::span<char const>, Prelude);

namespace impl {

template <util::fixed_string fmt_string, Location loc, typename... Args>
Message format(std::span<char const> data, Prelude meta) {
  // auto fmt = std::format_string<Args...>{fmt_string.to_sv()};

  auto reader  = message::MessageView{data};
  auto message = 
  std::string{fmt_string.to_sv()}; 
  // std::format(fmt, deserialize<Args>(reader)...);
  return Message{.meta = meta, .text = message, .location = loc};
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