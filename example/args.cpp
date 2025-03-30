#include <erl/config/args.hpp>
#include <erl/print.hpp>

struct Args : erl::clap {
  std::string text;
  
  [[=expect(value < 10)]] 
  int times = 5;

  [[= option]] 
  [[= description("sets logging verbosity")]]
  void verbosity(unsigned level) {
    std::println("verbosity: {}", level);
  }

  [[= option]] 
  [[= description("print program version")]]
  static void version() {
    auto current_version = 1;
    std::println("v{}", current_version);
    std::exit(1);
  }
};

int main(int argc, const char** argv) {
  auto opts = erl::parse_args<Args>({argv, argv+argc});
  erl::println("text: {text} \ntimes: {times}", opts);
}