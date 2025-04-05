#include <erl/_impl/log/logger.hpp>
#include <map>
#include <thread>
#include <vector>

#include <erl/print>
#include <erl/_impl/log/sinks/terminal.hpp>
#include <erl/info>

#include <iostream>
#include "erl/_impl/log/format/log.hpp"
#include "erl/_impl/log/message.hpp"

void bench() {
  // const size_t queueSize = 100000;
  constexpr static int64_t iters = 1'000'000;
  auto logger                    = erl::logging::Logger();

  // auto start = std::chrono::steady_clock::now();
  auto ingress = [] {
    std::map<int, std::vector<int>> foo = {{1, {2, 3, 47, 2}}, {23, {7, 8}}, {2394, {0xff, 3, 7, 3}}};
    for (int i = 0; i < iters; ++i) {
      erl::log("foo {} bar {}", i, 420);
      erl::log("foo {} bar {}", i, 420);
      erl::log("foo {} bar {}", i, 420);
      erl::log("foo {} bar", foo);
      erl::log("foo {} bar", foo);
    }
  };
  std::jthread input_thread[] = {
      std::jthread(ingress), std::jthread(ingress), std::jthread(ingress), std::jthread(ingress),
      std::jthread(ingress), std::jthread(ingress), std::jthread(ingress), std::jthread(ingress),
      std::jthread(ingress), std::jthread(ingress), std::jthread(ingress), std::jthread(ingress),
      std::jthread(ingress), std::jthread(ingress), std::jthread(ingress), std::jthread(ingress),
      // std::jthread(ingress), std::jthread(ingress), std::jthread(ingress), std::jthread(ingress),
      // std::jthread(ingress), std::jthread(ingress), std::jthread(ingress), std::jthread(ingress),
      // std::jthread(ingress), std::jthread(ingress), std::jthread(ingress), std::jthread(ingress),
      // std::jthread(ingress), std::jthread(ingress), std::jthread(ingress), std::jthread(ingress),
  };
  for (auto&& thread : input_thread) {
    thread.join();
  }

  logger.stop();
  // auto stop = std::chrono::steady_clock::now();

  // std::cout << iters * 1000000000 /
  //                  std::chrono::duration_cast<std::chrono::nanoseconds>(stop -
  //                                                                       start)
  //                      .count()
  //           << " messages/s\n";
}

#include <ctime>
#include <iostream>
#include <chrono>
#include <sys/time.h>

#include <erl/_impl/log/format/color.hpp>

int main() {
  using namespace std::chrono_literals;
  using namespace erl::logging;
  erl::this_thread.set_name("main");

  auto logger = Logger();
  auto color_map = std::unordered_map<Severity, std::string_view>{
    {Severity::DEBUG, erl::style::Reset},
    {Severity::INFO, erl::style::Reset},
    {Severity::WARNING, erl::fg::Yellow},
    {Severity::ERROR, erl::fg::Red},
    {Severity::FATAL, erl::fg::Red | erl::style::Bold},
  };

  logger.add_sink(new Terminal(
    "{timestamp:%H:%M:%S} {color} {level:<8} {thread:<8} {file_name}:{line}:{column} {reset} | {message}",
                               Severity::INFO, color_map));

  std::map<int, std::vector<int>> foo = {{1, {2, 3, 47, 2}}, {23, {7, 8}}, {2394, {0xff, 3, 7, 3}}};
  erl::warning("foo {} bar", erl::bg::Red(foo));

  auto style = erl::fg::Red | erl::style::Bold;
  erl::error("zoinks {} boings {} doings {}", style, erl::style::Reset, erl::fg::Default("oings"));

  std::jthread(
      [](auto parent) {
        erl::this_thread.set_name("child").set_parent(parent);
        erl::fatal("foo {}", 420);
      },
      erl::this_thread);
  
  erl::log("foo");
  logger.stop();
}