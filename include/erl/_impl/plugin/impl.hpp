#pragma once
#include "cinterface.h"
#include <erl/version.h>
#include <erl/autoload.hpp>
#include <stdexcept>

namespace erl::plugin {
struct Interface {
  PluginInfo* plugin_info;

  void (*plugin_init)();
  void (*plugin_fini)();
};

struct PluginError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct Plugin : private Library<Interface> {
  PluginInfo info{};

  explicit Plugin(std::string_view name) 
  : Library<Interface>(name) {
    if ((*this)->plugin_info->api_version > ERL_API_VERSION) {
      throw PluginError("Plugin was compiled against newer api version.");
    }
    info = *(*this)->plugin_info;
    (*this)->plugin_init();
  }

  ~Plugin() {
    (*this)->plugin_fini();
  }
};
}