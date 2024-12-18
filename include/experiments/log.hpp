#pragma once
#include <string_view>
#include "message.hpp"
#include <format>

namespace slo {
struct Logger {
  template <typename... Args>
  static std::string format(rpc::MessageParser<>& message) {
    auto fmt = message.get<std::string>();
    return std::vformat(fmt, std::make_format_args(message.get<Args>()...));
  }

  static void enqueue(rpc::MessageWriter<> const& msg) {}
};

template <typename... Args>
void log(std::string_view fmt, Args&&... args) {
  auto msg        = rpc::MessageWriter<>{};
  auto format_fnc = &Logger::format<Args...>;
  msg.add_field(static_cast<std::uintptr_t>(format_fnc));
  msg.add_field(fmt);
  (msg.add_field(args), ...);
  Logger::enqueue(msg);
}
}  // namespace slo