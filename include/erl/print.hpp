#pragma once
#include <erl/log/logger.hpp>
#include <erl/log/message.hpp>

namespace erl {
template <typename... Args>
void debug(logging::FormatString<Args...> fmt, Args&&... args) {
  logging::emit_message(logging::DEBUG, fmt.format, std::forward<Args>(args)...);
}

template <typename... Args>
void log(logging::FormatString<Args...> fmt, Args&&... args) {
  logging::emit_message(logging::INFO, fmt.format, std::forward<Args>(args)...);
}

template <typename... Args>
void warning(logging::FormatString<Args...> fmt, Args&&... args) {
  logging::emit_message(logging::WARNING, fmt.format, std::forward<Args>(args)...);
}

template <typename... Args>
void error(logging::FormatString<Args...> fmt, Args&&... args) {
  logging::emit_message(logging::ERROR, fmt.format, std::forward<Args>(args)...);
}

template <typename... Args>
void fatal(logging::FormatString<Args...> fmt, Args&&... args) {
  logging::emit_message(logging::FATAL, fmt.format, std::forward<Args>(args)...);
}
}