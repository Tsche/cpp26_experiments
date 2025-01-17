#include <slo/config/args.hpp>
#include <filesystem>

struct Args : slo::clap {
  [[=shorthand("-c")]]
  std::filesystem::path config;

  [[=callback]]
  [[=shorthand("-h")]]
  void help() const {

  }
};


int main(int argc, const char** argv) {
  auto args = Args{}.parse(argc, argv);
  // auto args = slo::parse_args<Args>(argc, argv);
}