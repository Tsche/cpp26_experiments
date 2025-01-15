#pragma once
#include <slo/logging/logger.hpp>
#include <slo/logging/message.hpp>

namespace slo {
template <typename... Args>
void debug(logging::Formatter<Args...> fmt, Args&&... args) {
  logging::emit_message(logging::DEBUG, fmt.format, std::forward<Args>(args)...);
}

template <typename... Args>
void print(logging::Formatter<Args...> fmt, Args&&... args) {
  logging::emit_message(logging::INFO, fmt.format, std::forward<Args>(args)...);
}

template <typename... Args>
void warning(logging::Formatter<Args...> fmt, Args&&... args) {
  logging::emit_message(logging::WARNING, fmt.format, std::forward<Args>(args)...);
}

template <typename... Args>
void error(logging::Formatter<Args...> fmt, Args&&... args) {
  logging::emit_message(logging::ERROR, fmt.format, std::forward<Args>(args)...);
}

template <typename... Args>
void fatal_error(logging::Formatter<Args...> fmt, Args&&... args) {
  logging::emit_message(logging::FATAL, fmt.format, std::forward<Args>(args)...);
}
}