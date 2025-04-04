#pragma once
#include <vector>
#include <unordered_map>

#include <erl/_impl/net/message/buffer.hpp>
#include <erl/rpc.hpp>
#include <erl/_impl/rpc/proxy.hpp>
#include <erl/clock.hpp>

#include "sinks/sink.hpp"
#include "message.hpp"

namespace erl::logging {
namespace _impl {
struct ThreadEventHelper {
  ThreadEventHelper() noexcept;
  ~ThreadEventHelper() noexcept;

  ThreadEventHelper(ThreadEventHelper const&)            = delete;
  ThreadEventHelper(ThreadEventHelper&&)                 = delete;
  ThreadEventHelper& operator=(ThreadEventHelper const&) = delete;
  ThreadEventHelper& operator=(ThreadEventHelper&&)      = delete;
};
inline thread_local ThreadEventHelper const thread_helper{};
}  // namespace _impl

class LoggingService {
  std::vector<Sink*> sinks;
  std::unordered_map<std::uint64_t, CachedThreadInfo> threads;

  CachedThreadInfo thread_get_info(std::uint64_t id);
  void handle_print(std::span<char const> data);

  void dispatch(auto fnc, auto&&... args) {
    for (auto&& sink : sinks) {
      (sink->*fnc)(std::forward<decltype(args)>(args)...);
    }
  }

  static LoggingEvent make_prelude(erl::logging::Severity severity, erl::logging::formatter_type formatter);

public:
  using policy       = rpc::Annotated;
  using message_type = erl::message::HybridBuffer<58>;
  using protocol     = rpc::RPCProtocol<message_type>;

  [[= rpc::callback]] void spawn(timestamp_t timestamp, std::uint64_t thread);
  [[= rpc::callback]] void exit(timestamp_t timestamp, std::uint64_t thread);
  [[= rpc::callback]] void rename(timestamp_t timestamp, std::uint64_t thread, std::string_view name);
  [[= rpc::callback]] void set_parent(timestamp_t timestamp, std::uint64_t thread, std::uint64_t parent_id);
  [[= rpc::callback]] void add_sink(Sink* sink);
  [[= rpc::callback]] void remove_sink(Sink* sink);

  template <typename... Args>
  [[= rpc::handler(^^LoggingService::handle_print)]] static auto print(erl::logging::Severity severity,
                                                                       erl::logging::formatter_type formatter,
                                                                       Args&&... args) {
    auto message   = message_type{};
    serialize(make_prelude(severity, formatter), message);
    (serialize(std::forward<Args>(args), message), ...);
    return message;
  }
};

namespace _impl {
  using queue_type = erl::EventQueue<erl::logging::LoggingService::message_type>;
}

class Logger : public erl::rpc::Proxy<LoggingService, decltype(std::declval<_impl::queue_type>().make_client())> {
  static auto& message_queue() {
    static erl::EventQueue<erl::logging::LoggingService::message_type> queue{};
    return queue;
  }

  using base = erl::rpc::Proxy<LoggingService, decltype(std::declval<_impl::queue_type>().make_client())>;
public:
  static auto handle_messages() {
    auto server  = message_queue().make_server();
    auto service = LoggingService{};
    server.run(service);
  }

  static auto& client() {
    static auto handle = message_queue().make_client();
    static auto remote = erl::rpc::make_proxy<LoggingService>(&handle);
    return remote;
  }

  static void shutdown() { message_queue().make_client().kill(); }

  Logger() : base{client()} {}
};
}  // namespace erl::logging