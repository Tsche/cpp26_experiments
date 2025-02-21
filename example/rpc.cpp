#include <unistd.h>
#include <cstddef>
#include <cassert>
#include <cstdint>
#include <print>

#include <erl/rpc/rpc.hpp>
#include <erl/rpc/protocol.hpp>
#include <erl/queue/mpmc_bounded.hpp>
#include <erl/queue/spsc_bounded.hpp>
#include <erl/reflect.hpp>
#include <thread>
#include "erl/net/message/reader.hpp"

template <typename Proto>
struct BlockingCall {
  template <typename R, typename Self, typename... Args>
  auto call(this Self&& self, std::size_t index, Args&&... args) {
    auto request = Proto::request(index, std::forward<Args>(args)...);
    self.send(request);
    auto response = self.recv();
    return Proto::template read_response<R>(index, std::span<char const>{response});
  }

  template <typename Self, typename S>
  void handle(this Self&& self, S&& service, std::span<char const> message) {
    auto reply = Proto::dispatch(std::forward<S>(service), std::span<char const>{message});
    std::forward<Self>(self).send(reply);
  }
};

template <typename Proto>
struct EventCall {
  template <typename R, typename Self, typename... Args>
  void call(this Self&& self, std::size_t index, Args&&... args) {
    auto request = Proto::request(index, std::forward<Args>(args)...);
    std::forward<Self>(self).send(request);
  }

  template <typename Self, typename S>
  void handle(this Self&& self, S&& service, std::span<char const> message) {
    Proto::dispatch(std::forward<S>(service), std::span<char const>{message});
  }
};

template <typename MsgType>
struct RPCProtocol {
  using message_type = MsgType;
  using index_type   = std::uint16_t;

  template <typename... Args>
  static message_type request(index_type index, Args&&... args) {
    auto message = message_type{};
    erl::serialize(index, message);
    (erl::serialize(std::forward<Args>(args), message), ...);
    return message;
  }

  template <typename S>
  static message_type dispatch(S&& service, std::span<char const> message) {
    auto reader                      = erl::message::MessageView{message};
    auto index                       = erl::deserialize<index_type>(reader);
    auto remainder                   = reader.buffer.subspan(reader.cursor);
    constexpr static auto dispatcher = erl::rpc::impl::get_dispatcher<S, RPCProtocol>();
    return dispatcher(std::forward<S>(service), index, remainder);
  }

  template <typename... Ts>
  static message_type make_response(index_type index, Ts&&... value) {
    auto message = message_type{};
    erl::serialize(index, message);
    (erl::serialize(std::forward<Ts>(value), message), ...);
    return message;
  }

  template <typename T>
  static T read_response(index_type expected_index, std::span<char const> message) {
    auto reader = erl::message::MessageView{message};
    auto index  = erl::deserialize<index_type>(reader);
    assert(index == expected_index);
    if constexpr (!std::same_as<T, void>) {
      return erl::deserialize<T>(reader);
    }
  }
};

template <typename T>
concept is_queue = requires(T obj) {
  typename T::element_type;
  { obj.push(typename T::element_type{}) } -> std::same_as<void>;
  { obj.pop() } -> std::same_as<typename T::element_type>;
};

template <typename U, typename V>
struct Client {
  U* in;
  V* out;

  explicit Client(U* in, V* out) : in(in), out(out) {}

  void send(auto const& message)
    requires(is_queue<U>)
  {
    in->push(message);
  }

  auto recv()
    requires(is_queue<V>)
  {
    return out->pop();
  }

  void kill()
    requires(is_queue<U>)
  {
    auto goodbye_message = typename U::element_type{};
    in->push(goodbye_message);
  }
};

template <is_queue U>
Client(U*, std::nullptr_t) -> Client<U, void>;

template <is_queue V>
Client(std::nullptr_t, V*) -> Client<void, V>;

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

template <typename... T>
struct Mixin : T... {};

template <template <typename> class Protocol>
struct Pipe {
  using message       = erl::message::HybridBuffer<58>;
  using protocol      = Protocol<message>;
  using message_queue = erl::queues::BoundedSPSC<message, 64>;
  using call_type     = BlockingCall<protocol>;

  message_queue in{};
  message_queue out{};

  auto make_server() { return Server{Mixin{Client{&out, &in}, call_type{}}}; }

  auto make_client() { return Mixin{Client{&in, &out}, call_type{}}; }
};

template <typename T>
struct Null {
  Null() = default;
  Null(auto&&) {}

  T pop() {
    std::unreachable();
    return {};
  }

  void push(auto&&) { return; }
};

template <template <typename> class Protocol>
struct EventQueue {
  using message       = erl::message::HybridBuffer<58>;
  using protocol      = Protocol<message>;
  using call_type     = EventCall<protocol>;
  using message_queue = erl::queues::BoundedMPMC<message, 64>;

  message_queue events{};

  auto make_server() { return Server{Mixin{Client{nullptr, &events}, call_type{}}}; }

  auto make_client() { return Mixin{Client{&events, nullptr}, call_type{}}; }
};

struct TestService {
  int echo(int num) { return num; }
  void print(int num) { std::println("{}", num); }
};

int main() {
  // auto pipe = Pipe<RPCProtocol>();
  auto pipe = EventQueue<RPCProtocol>{};

  auto x = std::jthread([&] {
    auto service = TestService{};
    auto server  = pipe.make_server();
    server.run(service);
  });

  auto client = pipe.make_client();
  auto remote = erl::rpc::make_proxy<TestService>(&client);

  remote.echo(3);
  // std::println("{}", result);
  remote.print(42);
  remote.print(24);
  client.kill();
}

// #include <erl/log/logger.hpp>
// struct ThreadInfoService {
//   void spawn(std::uint64_t thread);
//   void exit(std::uint64_t thread);
//   void rename(std::uint64_t thread, std::string_view name);
//   void set_parent(std::uint64_t thread, std::uint64_t parent);
// };

// template <typename T>
// struct LoggingProxy : erl::rpc::Proxy<ThreadInfoService, T> {
//   template <typename... Args>
//   void print(erl::logging::Severity severity, erl::logging::formatter_type formatter, Args&&... args) {
//     auto* client = [:erl::meta::get_nth_member(^^erl::rpc::Proxy<ThreadInfoService, T>, 0):];
//     client->send(std::vector<char>{});
//   }
// };

// template <typename T>
// struct LoggingProtocol : RPCProtocol<T> {
//   using protocol = RPCProtocol<T>;

//   template <typename S>
//   static protocol::message_type dispatch(S&& service, std::span<char const> message) {
//     auto reader                      = erl::message::MessageView{message};
//     auto index                       = erl::deserialize<typename protocol::index_type>(reader);
//     auto remainder                   = reader.buffer.subspan(reader.cursor);
//     if (index == -1){
//       handle_print(std::forward<S>(service), remainder);
//     } else {
//       constexpr static auto dispatcher = erl::rpc::impl::get_dispatcher<S, LoggingProtocol>();
//       return dispatcher(std::forward<S>(service), index, remainder);
//     }
//   }

//   template <typename S>
//   static void handle_print(S&& service, std::span<char const> message){

//   }
// };

// struct Logger {
// static auto& event_queue() {
//   static EventQueue<LoggingProtocol> queue{};
//   return queue;
// }

// void run(){
//   ThreadInfoService thread_service;
//   auto server = event_queue().make_server();
//   server.run(thread_service);
// }

// };
