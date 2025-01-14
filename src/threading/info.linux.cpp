#include <cstdint>
#include <slo/threading/info.hpp>
#include <pthread.h>

constexpr static auto max_name_length = 64;

namespace slo::threading {
Info::Info() noexcept : id(pthread_self()) {}
Info::Info(std::uint64_t handle) : id(handle) {}

void Info::set_name(std::string const& name) const {
  pthread_setname_np(id, name.c_str());
}

std::string Info::get_name() const {
  char buffer[max_name_length + 1]{};
  auto retcode = pthread_getname_np(id, buffer, max_name_length);
  if (retcode != 0) {
    return {};
  }
  return std::string{buffer};
}
}