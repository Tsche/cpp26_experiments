#pragma once
#include <erl/log/logger.hpp>
#include <erl/log/message.hpp>
#include <erl/log/format.hpp>

namespace erl {
template <typename... Args>
void debug(logging::FormatString<Args...> fmt, Args&&... args) {
  logging::Logger::client().print(logging::DEBUG, fmt.format, std::forward<Args>(args)...);
}

template <typename... Args>
void log(logging::FormatString<Args...> fmt, Args&&... args) {
  logging::Logger::client().print(logging::INFO, fmt.format, std::forward<Args>(args)...);
}

template <typename... Args>
void warning(logging::FormatString<Args...> fmt, Args&&... args) {
  logging::Logger::client().print(logging::WARN, fmt.format, std::forward<Args>(args)...);
}

template <typename... Args>
void error(logging::FormatString<Args...> fmt, Args&&... args) {
  logging::Logger::client().print(logging::ERROR, fmt.format, std::forward<Args>(args)...);
}

template <typename... Args>
void fatal(logging::FormatString<Args...> fmt, Args&&... args) {
  logging::Logger::client().print(logging::FATAL, fmt.format, std::forward<Args>(args)...);
}
}