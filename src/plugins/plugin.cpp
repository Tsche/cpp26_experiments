#include <erl/plugin>

namespace erl {

Plugin load_plugin(std::string_view name, std::filesystem::path base) {
  //TODO
  if (base.empty()){
    return Plugin(name);
  }
  auto expanded_path = base / name;
  return Plugin(expanded_path.string());
}
}  // namespace erl