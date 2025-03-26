#include <thread>
#include <unordered_map>
#include <erl/print.hpp>
#include <erl/log/sinks/terminal.hpp>
#include <erl/log/format/color.hpp>
#include <erl/thread.hpp>

constexpr static auto message_format =
    "[{timestamp:%H:%M:%S}] [{color}{level:>5}{reset}] [{file_name}:{line}:{column}] {message}";

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

  Logger::shutdown();
}