#include <slo/log/logger.hpp>
#include <map>
#include <thread>
#include <vector>

#include <slo/print.hpp>
#include <slo/log/sinks/terminal.hpp>
#include <slo/threading/info.hpp>

int main() {
  using namespace std::chrono_literals;
  slo::this_thread.set_name("main");

  auto logger = slo::logging::Logger();
  logger.add_sink<slo::logging::Terminal>();

  std::map<int, std::vector<int>> foo = {{1, {2, 3, 47, 2}}, {23, {7, 8}}, {2394, {0xff, 3, 7, 3}}};
  slo::print("foo {} bar", foo);
  std::this_thread::sleep_for(1s);
  slo::print("foo {} bar {}", foo, 420);
  logger.stop();
}