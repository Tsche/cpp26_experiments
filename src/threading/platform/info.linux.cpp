#include <pthread.h>

#include "info.h"

constexpr static auto max_name_length = 64;

namespace erl::thread::impl {
std::uint64_t current() noexcept {
  return pthread_self();
}

void set_name(std::uint64_t id, std::string const& name) {
  pthread_setname_np(id, name.c_str());
}

std::string get_name(std::uint64_t id) {
  char buffer[max_name_length + 1]{};
  auto retcode = pthread_getname_np(id, &buffer[0], max_name_length);
  if (retcode != 0) {
    return {};
  }
  return std::string{&buffer[0]};
}
}