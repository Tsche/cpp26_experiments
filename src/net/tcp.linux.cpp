#include <string>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <erl/net/transport/tcp.hpp>

namespace erl::tcp {
Socket::Socket() : handle(-1) {}
Socket::~Socket() {
  if (handle > 0) {
    ::close(handle);
  }
}

bool Socket::is_valid() const {
  return handle > 0;
}

void Client::connect(std::string_view address, unsigned port) {
  handle = ::socket(AF_INET, SOCK_STREAM, 0);
  if (handle < 0) {
    return;
  }

  sockaddr_in addr{};
  addr.sin_family      = AF_INET;
  addr.sin_addr.s_addr = inet_addr(std::string(address).c_str());
  addr.sin_port        = port;
  if (::connect(handle, (sockaddr*)&addr, sizeof(addr)) != 0) {
    handle = -1;
  }
}

void Client::send(std::span<char const> message) const {
  ::write(handle, message.data(), message.size());
}

void Client::receive(char* buffer, std::size_t amount) const {
  std::size_t total_read = 0;
  do {
    auto amount_read = ::read(handle, buffer, amount - total_read);
    if (amount_read <= 0){
      return;
    }
    total_read += amount_read;
  } while(total_read < amount);
}

void Server::listen(unsigned short port, unsigned max_connections) {
  handle = socket(AF_INET, SOCK_STREAM, 0);
  if (handle < 0) {
    return;
  }

  sockaddr_in addr{};
  addr.sin_family      = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port        = port;
  if (bind(handle, (sockaddr*)&addr, sizeof(addr)) < 0) {
    handle = -1;
    return;
  }

  ::listen(handle, int(max_connections));
}
Client Server::accept(){
  sockaddr_in addr{};
  socklen_t addr_len;
  auto client = ::accept(handle, (sockaddr*)&addr, &addr_len);
  if (client < 0){
    // TODO error handling?
    return {};
  }

  return Client(client);
}
}