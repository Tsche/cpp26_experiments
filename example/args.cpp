#include <erl/args>
#include <erl/print>
constexpr inline auto value = erl::CLI::value;
constexpr inline auto option = erl::CLI::option;

struct [[= erl::CLI::description("Example program.")]] Args : erl::CLI {
  std::string text;
  [[=expect(2 < value < 17)]] 
  int times = 5;

  [[= option]] 
  [[= erl::CLI::description("sets logging verbosity")]]
  void verbosity([[=expect(value < 5)]] unsigned level) {
    std::println("verbosity: {}", level);
  }

  [[= option]]
  static void test(int x, bool b = false){
    std::println("{} {}", x, b);
  }

  [[= option]] 
  [[= erl::CLI::description("print program version")]]
  static void version() {
    auto current_version = 1;
    std::println("v{}", current_version);
    std::exit(1);
  }
};

int main(int argc, const char** argv) {
  auto opts = erl::parse_args<Args>({argv, argv+argc});
  // auto opts = erl::parse_args<Args>({"", "t"});
  erl::println("text: {text} \ntimes: {times}", opts);
}