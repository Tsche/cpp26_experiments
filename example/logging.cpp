#include <slo/logging/logger.hpp>
#include <iostream>
#include <stop_token>
#include <thread>
#include <chrono>

#include <slo/print.hpp>
#include <slo/logging/sinks/terminal.hpp>
#include "slo/threading/info.hpp"

int main(){
  auto logger = slo::logging::Logger();
  logger.add_sink<slo::logging::Terminal>();

  std::string foo = "4 2";
  slo::print("foo {} bar {}", foo, 42);

  using namespace std::chrono_literals;
  std::this_thread::sleep_for(1s);

  slo::this_thread.set_name("main");
  slo::warning("zoinks");
  slo::warning("zoinks");
  slo::print("foo {} bar {}", foo, 42);

  logger.stop();
}