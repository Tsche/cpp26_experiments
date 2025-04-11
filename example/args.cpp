#include <erl/args>
#include <erl/print>

struct [[= erl::CLI::description("Example program.")]] Args : erl::CLI {
  std::string text;
  [[=expect(2 < value < 17)]] 
  int times = 5;

  [[= option]] 
  [[= description("sets logging verbosity")]]
  void verbosity([[=expect(value < 5)]] unsigned level) {
    std::println("verbosity: {}", level);
  }

  [[= option]]
  static void test(int x, bool b = false){
    std::println("{} {}", x, b);
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