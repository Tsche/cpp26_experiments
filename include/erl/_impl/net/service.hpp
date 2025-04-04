#pragma once
#include <span>


namespace erl::net {
template <typename C>
struct Server {
  C client;

  explicit Server(C client) : client(client) {}

  template <typename T>
  void run(T&& service) {
    while (true) {
      auto msg = client.recv();
      if (msg.size() == 0) {
        break;
      }

      client.handle(service, std::span<char const>{msg});
    }
  }
};
}