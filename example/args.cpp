#include <erl/config/args.hpp>
#include <filesystem>

struct Args : erl::clap {
  [[=shorthand("-c")]]
  std::filesystem::path config;

  [[=callback]]
  [[=shorthand("-h")]]
  void help() const {

  }
};


int main(int argc, const char** argv) {
  auto args = Args{}.parse(argc, argv);
  // auto args = erl::parse_args<Args>(argc, argv);
}