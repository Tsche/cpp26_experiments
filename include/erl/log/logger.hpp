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
#include <erl/threading/info.hpp>
#include <erl/queue/basic.hpp>

#include "message.hpp"
#include "format.hpp"
#include "sinks/sink.hpp"
#include <erl/queue/mpmc_bounded.hpp>
#include <erl/queue/spsc_bounded.hpp>

namespace erl::logging {

Message parse_message(std::span<char const>);
struct Logger {
  using message_type = message::HybridBuffer<64-12>;

  static auto& message_queue() {
    // static queues::BoundedSPSC<message_type, 64> queue{};
    static queues::BoundedMPMC<message_type, 64> queue{};
    // static BasicQueue<message_type> queue{};
    return queue;
  }

  template <typename U>
  static void send(U&& message) {
    message_queue().push(std::forward<U>(message));
  }

  void run() {
    erl::this_thread.set_name("logger");
    auto token = stop_source.get_token();
    // std::stop_callback on_stop{token, []() { message_queue().notify_all(); }};

    while (true) {
      auto msg = message_queue().pop(token);
      if (msg.is_empty()) {
        break;
      }

      dispatch(parse_message(msg.finalize()));
    }
  }

  void dispatch(Message const& message) {
    std::scoped_lock lock{mutex};
    for (auto&& sink : sinks) {
      sink->print(message);
    }
  }

  template <typename T, typename... Args>
  void add_sink(Args&&... args) {
    std::scoped_lock lock{mutex};
    sinks.emplace_back(new T{std::forward<Args>(args)...});
  }

  Logger() : thread(&Logger::run, this) {}
  void stop() { stop_source.request_stop(); }
  ~Logger() {
    if (thread.joinable()) {
      stop();
      thread.join();
    }
  }

private:
  std::stop_source stop_source;
  std::mutex mutex;
  std::thread thread;
  std::vector<std::unique_ptr<Sink>> sinks;
};

template <typename... Args>
void emit_message(Severity severity, formatter_type formatter, Args&&... args) {
  auto message = Logger::message_type{};
  auto prelude = Prelude{.severity = severity, .timestamp = std::time({}), .thread = erl::this_thread};
  serialize(prelude, message);
  serialize(std::uintptr_t(formatter), message);
  (serialize(std::forward<Args>(args), message), ...);
  Logger::send(message);
}

inline Message parse_message(std::span<char const> data) {
  auto reader  = message::MessageView{data};
  auto prelude = deserialize<Prelude>(reader);
  auto fnc_ptr = deserialize<std::uintptr_t>(reader);
  auto fnc     = reinterpret_cast<formatter_type>(fnc_ptr);
  return fnc(data.subspan(reader.cursor), prelude);
}
}  // namespace erl::logging