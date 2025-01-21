#pragma once
#include <span>
#include <string_view>

namespace slo::tcp {
#ifdef SLO_IS_WINDOWS
using native_handle = void*;
#else
using native_handle = int;
#endif
struct Socket {
  Socket();
  ~Socket();
  explicit Socket(native_handle handle) : handle(handle) {}
  [[nodiscard]] bool is_valid() const;

  explicit(false) operator bool(){ return is_valid(); }
protected: 
  native_handle handle;
};

struct Client : Socket {
public:
  Client() = default;
  explicit Client(native_handle handle) : Socket(handle) {}

  void connect(std::string_view address, unsigned port);

  void send(std::span<char const> message) const;
  void receive(char* buffer, std::size_t amount) const;
  
};

struct Server : Socket {
  Server();
  explicit Server(native_handle handle) : Socket(handle) {}

  void listen(unsigned short port, unsigned max_connections = 1);
  Client accept();
};
}  // namespace slo::transport