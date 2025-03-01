#pragma once
#include <cstdint>
#include <stop_token>
#include <thread>
#include <vector>
#include <memory>
#include <mutex>
#include <ctime>

#include <erl/reflect.hpp>
#include <erl/net/message/buffer.hpp>
#include <erl/thread.hpp>
#include <erl/queue/basic.hpp>

#include "erl/net/message/reader.hpp"
#include "message.hpp"
#include "format.hpp"
#include "sinks/sink.hpp"
#include <erl/queue/mpmc_bounded.hpp>
#include <erl/queue/spsc_bounded.hpp>

#include <print>

namespace erl::logging {
struct Logger {
  using message_type = message::HybridBuffer<64 - 12>;

  static auto& message_queue() {
    // static queues::BoundedSPSC<message_type, 64> queue{};
    static queues::BoundedMPMC<message_type, 64> queue{};
    // static BasicQueue<message_type> queue{};
    return queue;
  }

  static void send(EventKind event, std::span<char const> message);
  void run();

  void handle_message(message::MessageView& data);
  void handle_thread_event(message::MessageView& data);

  static void thread_spawn(std::uint64_t thread);
  static void thread_exit(std::uint64_t thread);
  static void thread_rename(std::uint64_t thread, std::string_view name);
  static void thread_set_parent(std::uint64_t thread, std::uint64_t parent);

  CachedThreadInfo thread_get_info(std::uint64_t id);

  void add_sink(Sink* sink) {
    std::scoped_lock lock{mutex};
    sinks.emplace_back(std::move(sink));
  }

  template <typename T, typename... Args>
  void add_sink(Args&&... args) {
    std::scoped_lock lock{mutex};
    sinks.emplace_back(new T{std::forward<Args>(args)...});
  }

  void dispatch(auto fnc, auto&&... args) {
    std::scoped_lock lock{mutex};
    for (auto&& sink : sinks) {
      ((*sink).*fnc)(std::forward<decltype(args)>(args)...);
    }
  }

  Logger() : thread(&Logger::run, this) {}
  void stop() { stop_source.request_stop(); }
  ~Logger() {
    if (thread.joinable()) {
      stop();
      thread.join();
    }

    for (auto const& [id, element] : threads) {
      dispatch(&Sink::exit, element);
    }
  }

private:
  std::stop_source stop_source;
  std::mutex mutex;
  std::thread thread;

  std::vector<std::unique_ptr<Sink>> sinks;
  std::unordered_map<std::uint64_t, CachedThreadInfo> threads;
};

namespace impl {
struct ThreadEventHelper {
  ThreadEventHelper() noexcept;
  ~ThreadEventHelper() noexcept;

  ThreadEventHelper(ThreadEventHelper const&)            = delete;
  ThreadEventHelper(ThreadEventHelper&&)                 = delete;
  ThreadEventHelper& operator=(ThreadEventHelper const&) = delete;
  ThreadEventHelper& operator=(ThreadEventHelper&&)      = delete;
};
inline thread_local ThreadEventHelper const thread_helper{};
}  // namespace impl

template <typename... Args>
void emit_message(Severity severity, formatter_type formatter, Args&&... args) {
  (void)impl::thread_helper;
  auto message   = Logger::message_type{};
  auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
  auto event = LoggingEvent{.severity    = severity,
                            .thread      = erl::this_thread,
                            .timestamp   = timestamp,
                            .handler     = formatter};
  serialize(event, message);
  (serialize(std::forward<Args>(args), message), ...);
  Logger::send(EventKind::LOGGING, message.finalize());
}

}  // namespace erl::logging