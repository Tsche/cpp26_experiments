#include <print>

#include <erl/log/logger.hpp>
#include <erl/log/message.hpp>
#include <erl/reflect.hpp>
#include "erl/thread.hpp"

namespace erl::logging {

void Logger::send(EventKind event, std::span<char const> message) {
  auto data = message_type{};
  serialize(event, data);
  data.write(message.data(), message.size());
  message_queue().push(data);
}

void Logger::run() {
  erl::this_thread.set_name("logger");
  auto token = stop_source.get_token();
  // std::stop_callback on_stop{token, []() { message_queue().notify_all(); }};

  while (true) {
    auto msg = message_queue().pop(token);
    if (msg.is_empty()) {
      break;
    }

    auto reader = message::MessageView{msg.finalize()};
    auto kind   = deserialize<EventKind>(reader);
    switch (kind) {
      using enum EventKind;
      case LOGGING: handle_message(reader); break;
      case THREAD_INFO: handle_thread_event(reader); break;
    }
  }
}

void Logger::handle_message(message::MessageView& data) {
  auto prelude = deserialize<LoggingEvent>(data);
  auto [location, text] = prelude.handler(data.buffer.subspan(data.cursor));
  auto timestamp = timestamp_t{std::chrono::system_clock::duration{prelude.timestamp}};
  auto message = Message{
    .severity = prelude.severity,
    .thread = thread_get_info(prelude.thread.id),
    .timestamp = timestamp,
    .location = location,
    .text = text
  };
  dispatch(&Sink::print, message);
}

void Logger::handle_thread_event(message::MessageView& data) {
  auto event = deserialize<ThreadEvent>(data);
  auto id    = deserialize<std::uint64_t>(data);
  switch (event) {
    using enum ThreadEvent;
    case SPAWN: {
      auto& info = threads[id];
      info.id = id;
      dispatch(&Sink::spawn, info);
      break;
    }
    case EXIT: {
      if (auto it = threads.find(id); it != threads.end()){
        dispatch(&Sink::exit, it->second);
      } else {
        dispatch(&Sink::exit, CachedThreadInfo{.id=id});
      }

      threads.erase(id);
      break;
    }
    case RENAME: {
      auto name = deserialize<std::string>(data);

      auto& info = threads[id];
      dispatch(&Sink::rename, info, name);
      info.name = name;

      break;
    }
    case SET_PARENT: {
      auto parent_id = deserialize<std::uint64_t>(data);

      auto& info = threads[id];
      auto& parent = threads[parent_id];
      if (parent.id != parent_id) {
        // parent just created
        parent.id = parent_id;
      }

      dispatch(&Sink::set_parent, info, parent);
      info.parent = parent_id;
      break;
    }
  }
}

void Logger::thread_spawn(std::uint64_t thread) {
  auto message = message_type{};
  serialize(ThreadEvent::SPAWN, message);
  serialize(thread, message);
  send(EventKind::THREAD_INFO, message.finalize());
}

void Logger::thread_exit(std::uint64_t thread) {
  auto message = message_type{};
  serialize(ThreadEvent::EXIT, message);
  serialize(thread, message);
  send(EventKind::THREAD_INFO, message.finalize());
}

void Logger::thread_rename(std::uint64_t thread, std::string_view name) {
  auto message = message_type{};
  serialize(ThreadEvent::RENAME, message);
  serialize(thread, message);
  serialize(name, message);
  send(EventKind::THREAD_INFO, message.finalize());
}

void Logger::thread_set_parent(std::uint64_t thread, std::uint64_t parent) {
  auto message = message_type{};
  serialize(ThreadEvent::SET_PARENT, message);
  serialize(thread, message);
  serialize(parent, message);
  send(EventKind::THREAD_INFO, message.finalize());
}

CachedThreadInfo Logger::thread_get_info(std::uint64_t thread) {
  auto it = threads.find(thread);
  if (it != threads.end()) {
    return it->second;
  }
  // todo generate if not found
  return {.id=thread};
}

namespace impl {
ThreadEventHelper::ThreadEventHelper() noexcept {
  Logger::thread_spawn(this_thread.id);
}
ThreadEventHelper::~ThreadEventHelper() noexcept {
  Logger::thread_exit(this_thread.id);
}
}  // namespace impl
}  // namespace erl::logging