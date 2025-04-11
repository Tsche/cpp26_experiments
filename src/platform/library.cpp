#include <string>
#include <string_view>


#if (defined(_WIN32) || defined(_WIN64))
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#    include <Windows.h>
#    undef WIN32_LEAN_AND_MEAN
#  else
#    include <Windows.h>
#  endif
#else
#  include <dlfcn.h>
#endif

#include <erl/autoload>

namespace erl::platform {
#if (defined(_WIN32) || defined(_WIN64))
namespace _impl {
void local_free(void* ptr) {
  ::LocalFree(ptr);
}
}  // namespace _impl
#endif

std::string get_last_error() {
#if (defined(_WIN32) || defined(_WIN64))
  DWORD error_id = GetLastError();
  if (error_id == 0) {
    return {};
  }

  LPTSTR buffer = nullptr;
  auto size =
      FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                     error_id, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&buffer, 0, NULL);

  if (size == 0) {
    return std::format("Invalid error code {}", error_id);
  }
  auto msg = std::unique_ptr<LPTSTR, void (*)(ptr)>(buffer, &_impl::local_free);
  return std::string(msg.get(), size);
#else
  // TODO this might not be thread-safe
  char const* error_msg = ::dlerror();
  if (error_msg == nullptr) {
    return {};
  }
  return error_msg;
#endif
}

void* load_library(std::string_view path) {
#if (defined(_WIN32) || defined(_WIN64))
  void* handle = (void*)::LoadLibraryExA(std::string{path}.c_str(), NULL, NULL);
#else
  void* handle = ::dlopen(std::string{path}.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
  if (handle == nullptr) {
    throw LibraryError(get_last_error());
  }
  return handle;
}

void unload_library(void* handle) {
#if (defined(_WIN32) || defined(_WIN64))
  ::FreeLibrary(handle);
#else
  ::dlclose(handle);
#endif
}

void* resolve_symbol(void* handle, std::string_view name) {
#if (defined(_WIN32) || defined(_WIN64))
  void* addr = (void*)::GetProcAddress(handle, std::string{name}.c_str());
#else
  void* addr = ::dlsym(handle, std::string{name}.c_str());
#endif

  if (addr == nullptr) {
    throw LibraryError(get_last_error());
  }
  return addr;
}
}