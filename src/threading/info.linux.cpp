#include <slo/threading/info.hpp>
#include <pthread.h>

constexpr static auto max_name_length = 64;

namespace slo {
ThreadInfo ThreadInfo::current() noexcept {
  return {pthread_self()};
}

void ThreadInfo::set_name(std::string const& name) const {
  pthread_setname_np(id, name.c_str());
}

std::string ThreadInfo::get_name() const {
  char buffer[max_name_length + 1]{};
  auto retcode = pthread_getname_np(id, &buffer[0], max_name_length);
  if (retcode != 0) {
    return {};
  }
  return std::string{&buffer[0]};
}
}