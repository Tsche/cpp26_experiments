#include <erl/plugin.hpp>

namespace erl {

Plugin load_plugin(std::string_view name) {
  return Plugin(name);
}
}  // namespace erl