#pragma once
#include <slo/logging/logger.hpp>

namespace slo {
template <typename... Args>
void debug(logging::Formatter<Args...> fmt, Args&&... args) {
  logging::emit_message(fmt.format, std::forward<Args>(args)...);
}

template <typename... Args>
void print(logging::Formatter<Args...> fmt, Args&&... args) {
  logging::emit_message(fmt.format, std::forward<Args>(args)...);
}

template <typename... Args>
void warning(logging::Formatter<Args...> fmt, Args&&... args) {
  logging::emit_message(fmt.format, std::forward<Args>(args)...);
}

template <typename... Args>
void error(logging::Formatter<Args...> fmt, Args&&... args) {
  logging::emit_message(fmt.format, std::forward<Args>(args)...);
}
}