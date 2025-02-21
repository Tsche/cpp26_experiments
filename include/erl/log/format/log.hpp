#pragma once
#include <cstdint>
#include <chrono>
#include <utility>
#include <filesystem>
#include <experimental/meta>

#include <erl/kwargs.hpp>
#include <erl/util/string.hpp>
#include <erl/util/meta.hpp>
#include <string>
#include "../message.hpp"

#include "color.hpp"

namespace erl::logging {
namespace formatter_impl {
struct MessageArgs {
  using time_point = std::chrono::time_point<std::chrono::local_t, std::chrono::microseconds>;

  std::uint8_t level_id;
  std::string level;

  std::uint64_t thread_id;
  std::string thread;

  time_point timestamp;

  std::string file;
  std::string file_name;
  std::string function;
  std::uint32_t line;
  std::uint32_t column;

  std::string message;

  std::string_view color = "";
  std::string_view reset = style::Reset;
};

using log_format_type = std::string (*)(MessageArgs const&);

template <util::fixed_string fmt>
std::string format(MessageArgs const& args) {
  return [:meta::expand(nonstatic_data_members_of(^^MessageArgs)):] >> [&]<std::meta::info... Members> {
    return std::format(fmt, args.[:Members:]...);
  };
}
}  // namespace formatter_impl

struct LogFormat {
  formatter_impl::log_format_type format;

  template <typename Tp>
    requires std::convertible_to<Tp const&, std::string_view>
  consteval explicit(false) LogFormat(Tp const& str) {
    auto parser = formatting::FmtParser{str};
    auto fmt    = parser.transform(meta::get_member_names<formatter_impl::MessageArgs>());
    format      = extract<formatter_impl::log_format_type>(substitute(^^formatter_impl::format, {meta::intern(fmt)}));
  }

  std::string operator()(Message const& msg, std::string_view color = "") const {
    std::string level;
    if (msg.severity > Severity::FATAL) {
      level = "INVALID";
    } else {
      level = meta::enumerator_names<Severity>[msg.severity];
    }

    using time_point = formatter_impl::MessageArgs::time_point;
    auto local_time = std::chrono::current_zone()->to_local(msg.timestamp);
    auto timestamp = time_point{local_time.time_since_epoch()};

    auto args = formatter_impl::MessageArgs{.level_id  = msg.severity,
                                            .level     = level,
                                            .thread_id = msg.thread.id,
                                            .thread    = msg.thread.name,
                                            .timestamp = timestamp,
                                            .file      = msg.location.file,
                                            .file_name = std::filesystem::path{msg.location.file}.filename(),
                                            .function  = msg.location.function,
                                            .line      = msg.location.line,
                                            .column    = msg.location.column,
                                            .message   = msg.text,
                                            .color     = color};
    return format(args);
  }
};
}  // namespace erl::logging