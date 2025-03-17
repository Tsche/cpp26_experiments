#pragma once
#include <algorithm>
#include <string_view>
#include <string>
#include <type_traits>
#include <utility>
#include <stdexcept>
#include <experimental/meta>

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

#include <erl/util/meta.hpp>

namespace erl {

struct LibraryError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

namespace platform {
#if (defined(_WIN32) || defined(_WIN64))
using handle_type = HINSTANCE;
using symbol_type = FARPROC;
#else
using handle_type = void*;
using symbol_type = void*;
#endif

#if (defined(_WIN32) || defined(_WIN64))
namespace _impl {
inline void local_free(void* ptr) {
  ::LocalFree(ptr);
}
}  // namespace _impl
#endif

inline std::string get_last_error() {
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

inline handle_type load_library(std::string_view path) {
#if (defined(_WIN32) || defined(_WIN64))
  handle_type handle = ::LoadLibraryExA(std::string{path}.c_str(), NULL, NULL);
#else
  handle_type handle = ::dlopen(std::string{path}.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
  if (!static_cast<bool>(handle)) {
    throw LibraryError(get_last_error());
  }
  return handle;
}

inline void unload_library(handle_type handle) {
#if (defined(_WIN32) || defined(_WIN64))
  ::FreeLibrary(handle);
#else
  ::dlclose(handle);
#endif
}

inline symbol_type get_symbol(handle_type handle, std::string_view name) {
#if (defined(_WIN32) || defined(_WIN64))
  symbol_type addr = ::GetProcAddress(handle, std::string{name}.c_str());
#else
  symbol_type addr = ::dlsym(handle, std::string{name}.c_str());
#endif

  if (!bool(addr)) {
    throw LibraryError(get_last_error());
  }
  return addr;
}

}  // namespace platform

template <typename Wrapper>
  requires(std::is_aggregate_v<Wrapper>)
struct Library {
private:
  platform::handle_type handle;
  Wrapper symbols;

  template <typename T>
  T load_symbol(std::string_view name) {
#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
    return reinterpret_cast<T>(platform::get_symbol(handle, name));
#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif
  }

  Wrapper load_symbols() {
    return [:meta::expand(nonstatic_data_members_of(^^Wrapper)):] >> [&]<auto... member> {
      return Wrapper{load_symbol<[:type_of(member):]>(identifier_of(member))...};
    };
  }

public:
  explicit Library(std::string_view path) : handle{platform::load_library(path)}, symbols(load_symbols()) {}
  ~Library() { platform::unload_library(handle); }

  Library(Library const&)            = delete;
  Library& operator=(Library const&) = delete;

  Library(Library&& other) noexcept : handle(other.handle), symbols(other.symbols) {
    other.handle  = nullptr;
    other.symbols = {};
  }

  Library& operator=(Library&& other) noexcept {
    if (this != &other) {
      std::swap(symbols, other.symbols);
      std::swap(handle, other.handle);
    }
    return *this;
  }

  Wrapper const& operator*() const { return symbols; }
  Wrapper const* operator->() const { return &symbols; }
};

}  // namespace erl
