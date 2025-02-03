#include <erl/log/logger.hpp>
#include <map>
#include <thread>
#include <vector>

#include <erl/print.hpp>
#include <erl/log/sinks/terminal.hpp>
#include <erl/threading/info.hpp>

#include <iostream>

void bench() {
  // const size_t queueSize = 100000;
  constexpr static int64_t iters = 1'000'000;
  auto logger = erl::logging::Logger();

  // auto start = std::chrono::steady_clock::now();
  auto ingress = []{
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
  for (auto&& thread : input_thread){
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

int main() {
  // bench();
  using namespace std::chrono_literals;
  erl::thread::set_name("main");

  auto logger = erl::logging::Logger();
  logger.add_sink<erl::logging::Terminal>();

  std::map<int, std::vector<int>> foo = {{1, {2, 3, 47, 2}}, {23, {7, 8}}, {2394, {0xff, 3, 7, 3}}};
  erl::log("foo {} bar", foo);
  
  std::this_thread::sleep_for(2s);
  erl::log("foo {} bar {}", 64, 420);
  logger.stop();
}