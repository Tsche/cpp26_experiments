#pragma once
#include <span>
#include <vector>

namespace slo::transport {
#ifdef SLO_IS_WINDOWS
using native_handle = void*;
#else
using native_handle = int;
#endif
struct TCPClient {
  TCPClient(std::string_view address, unsigned port);
  explicit TCPClient(native_handle handle);

  void send(std::span<char const> message);
  std::vector<char> receive(std::size_t amount);
};

struct TCPServer {
  void listen(unsigned port);
  TCPClient accept();
};
}  // namespace slo::transport