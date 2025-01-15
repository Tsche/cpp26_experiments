#pragma once
#include <cstdint>
#include <stop_token>
#include <thread>
#include <vector>
#include <memory>
#include <mutex>
#include <ctime>

#include <slo/reflect.hpp>
#include <slo/message.hpp>
#include <slo/message/buffered.hpp>
#include <slo/network/transport/queue.hpp>
#include <slo/threading/info.hpp>

#include "message.hpp"
#include "formatter.hpp"
#include "sinks/sink.hpp"

namespace slo::logging {

Message parse_message(std::span<char const>);
struct Logger {
  static auto& message_queue() {
    static transport::MessageQueue queue{};
    return queue;
  }

  static void send(std::span<char const> message) { message_queue().send(message); }
  void run() {
    slo::this_thread.set_name("logger");
    auto token = stop_source.get_token();
    std::stop_callback on_stop{token, []() { message_queue().notify_all(); }};

    while (true) {
      auto msg = message_queue().receive(token);
      if (msg.empty()) {
        break;
      }

      dispatch(parse_message(msg));
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
  auto message = MessageWriter<message::HybridBuffer<>>{};
  auto prelude = Prelude{.severity = severity,
                         .timestamp = std::time({}), 
                         .thread = slo::this_thread,
                         .formatter = std::uintptr_t(formatter), };
  serialize(prelude, message);
  (serialize(std::forward<Args>(args), message), ...);
  Logger::send(message.finalize());
}

inline Message parse_message(std::span<char const> data) {
  auto reader  = MessageReader{data};
  auto prelude = deserialize<Prelude>(reader);
  auto fnc     = reinterpret_cast<formatter_type>(prelude.formatter);
  return fnc(data.subspan(reader.cursor), prelude);
}
}  // namespace slo::logging