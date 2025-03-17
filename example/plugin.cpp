#include <print>
#include <erl/plugin.hpp>

int main() {
  auto plugin = erl::load_plugin("libdemo_plugin.so");

  std::println(
      "name        = {}\n"
      "version     = {}\n"
      "description = {}",
      plugin.info.name, plugin.info.version, plugin.info.description);
}