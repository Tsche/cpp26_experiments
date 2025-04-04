#include <thread>
#include <unordered_map>
#include <erl/print>
#include <erl/_impl/log/sinks/terminal.hpp>
#include <erl/_impl/log/format/color.hpp>
#include <erl/info>
#include <erl/print>
#include <erl/logging>
constexpr static auto message_format = 
  "[{timestamp:%H:%M:%S}] [{level:>5}] [{file_name}:{line}:{column}] {message}";

constexpr static auto colors = erl::color_map({
    {erl::logging::Severity::TRACE, erl::style::Reset},
    {erl::logging::Severity::DEBUG, erl::fg::Cyan},
    {erl::logging::Severity::INFO, erl::fg::Green},
    {erl::logging::Severity::WARN, erl::fg::Yellow},
    {erl::logging::Severity::ERROR, erl::fg::Red},
    {erl::logging::Severity::FATAL, erl::fg::Red | erl::style::Bold}});

int main() {
  using namespace erl::logging;

  auto terminal = erl::logging::Terminal(message_format, Severity::TRACE, colors);
  Logger::client().add_sink(&terminal);

  std::jthread _(Logger::handle_messages);

  erl::debug("boings");
  erl::log("bar");
  erl::warning("zoinks {}", -2);
  erl::error("foo {}", "oof");
  erl::fatal("oh no");
  
  auto local_logger = Logger{};
  local_logger.spawn(0, 1234);

  erl::println("size: {}", sizeof(Logger::client()));

  Logger::shutdown();
}