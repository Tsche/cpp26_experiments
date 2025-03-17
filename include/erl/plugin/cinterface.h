#ifndef ERL_PLUGIN_CINTERFACE
#define ERL_PLUGIN_CINTERFACE
struct PluginInfo {
  int api_version;
  char const* name;
  char const* version;
  char const* description;
};

#if defined(_WIN32) || defined(_WIN64)
#define $export __declspec(dllexport)
#else
#define $export
#endif

#endif