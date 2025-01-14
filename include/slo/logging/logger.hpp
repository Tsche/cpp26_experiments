#pragma once
#include <cstdint>
#include <stop_token>
#include <thread>
#include <vector>
#include <memory>
#include <mutex>

#include <slo/reflect.hpp>
#include <slo/message.hpp>
#include <slo/message/buffered.hpp>
#include <slo/transport/queue.hpp>
#include <slo/threading/info.hpp>

#include "formatter.hpp"
#include "sinks/sink.hpp"

namespace slo::logging {

Message parse_message(std::span<char>);
struct Logger {
  static auto& message_queue() {
    static transport::MessageQueue queue{};
    return queue;
  }

  static void send(std::span<char> message) { message_queue().send(message); }
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
void emit_message(formatter_type formatter, Args&&... args) {
  auto message = MessageWriter<message::HeapBuffer>{};
  serialize(std::uintptr_t(formatter), message);
  serialize(slo::threading::Info().get_id(), message);

  (serialize(std::forward<Args>(args), message), ...);
  Logger::send(message.finalize());
}

inline Message parse_message(std::span<char> data) {
  auto reader  = MessageReader{data};
  auto fnc_ptr = deserialize<std::uintptr_t>(reader);
  auto fnc     = reinterpret_cast<formatter_type>(fnc_ptr);
  auto thread  = deserialize<std::uint64_t>(reader);

  auto message = fnc(data.subspan(sizeof(fnc_ptr) + sizeof(thread)));
  message.thread = thread;
  return message;
}
}  // namespace slo::logging