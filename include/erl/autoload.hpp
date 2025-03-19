#pragma once
#include <algorithm>
#include <string_view>
#include <string>
#include <type_traits>
#include <utility>
#include <stdexcept>
#include <experimental/meta>

#include <erl/util/meta.hpp>

namespace erl {

struct LibraryError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

namespace platform {
std::string get_last_error();

void* load_library(std::string_view path);
void unload_library(void* handle);
void* resolve_symbol(void* handle, std::string_view name);

template <typename T>
T* resolve_symbol(void* handle, std::string_view name) {
#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
    return (T*)platform::resolve_symbol(handle, name);
#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif
  }
}  // namespace platform

template <typename Wrapper>
  requires(std::is_aggregate_v<Wrapper>)
struct Library {
private:
  void* handle;
  Wrapper symbols;

  Wrapper load_symbols() {
    return [:meta::expand(nonstatic_data_members_of(^^Wrapper)):] >> [&]<auto... member> {
      return Wrapper{platform::resolve_symbol<[:remove_pointer(type_of(member)):]>(handle, identifier_of(member))...};
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
