#include <cstdio>
#include <erl/version.h>
#include <erl/_impl/plugin/cinterface.h>
extern "C" {
$export PluginInfo plugin_info{.api_version = ERL_API_VERSION,
                               .name        = "demo plugin",
                               .version     = "v1.0.0",
                               .description = "simple demo plugin"};

$export void plugin_init() {
  std::puts("plugin_init()");
}

$export void plugin_fini() {
  std::puts("plugin_fini()");
}
}