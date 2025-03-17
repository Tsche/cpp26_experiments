#include <chrono>

#include <erl/log/logger.hpp>
#include <erl/log/message.hpp>
#include <erl/net/message/reader.hpp>

namespace erl::logging {
namespace _impl {
ThreadEventHelper::ThreadEventHelper() noexcept {
  Logger::client().spawn(this_thread.id);
}
ThreadEventHelper::~ThreadEventHelper() noexcept {
  Logger::client().exit(this_thread.id);
}
}  // namespace _impl

CachedThreadInfo LoggingService::thread_get_info(std::uint64_t thread) {
  auto it = threads.find(thread);
  if (it != threads.end()) {
    return it->second;
  }
  // todo generate if not found
  return {.id = thread};
}

void LoggingService::handle_print(std::span<char const> data) {
  auto reader           = erl::message::MessageView{data};
  auto event            = deserialize<LoggingEvent>(reader);
  auto [location, text] = event.handler(reader.buffer.subspan(reader.cursor));
  auto timestamp        = timestamp_t{std::chrono::system_clock::duration{event.timestamp}};
  auto message          = Message{.severity  = event.severity,
                                  .thread    = thread_get_info(event.thread.id),
                                  .timestamp = timestamp,
                                  .location  = location,
                                  .text      = text};
  dispatch(&Sink::print, message);
}

void LoggingService::spawn(std::uint64_t thread) {
  auto& info = threads[thread];
  info.id    = thread;
  dispatch(&Sink::spawn, info);
}

void LoggingService::exit(std::uint64_t thread) {
  if (auto it = threads.find(thread); it != threads.end()) {
    dispatch(&Sink::exit, it->second);
  } else {
    dispatch(&Sink::exit, CachedThreadInfo{.id = thread});
  }

  threads.erase(thread);
}

void LoggingService::rename(std::uint64_t thread, std::string_view name) {
  auto& info = threads[thread];
  dispatch(&Sink::rename, info, name);
  info.name = name;
}

void LoggingService::set_parent(std::uint64_t thread, std::uint64_t parent_id) {
  auto& info   = threads[thread];
  auto& parent = threads[parent_id];
  if (parent.id != parent_id) {
    // parent just created
    parent.id = parent_id;
  }

  dispatch(&Sink::set_parent, info, parent);
  info.parent = parent_id;
}

void LoggingService::add_sink(Sink* sink) {
  sinks.push_back(sink);
}

void LoggingService::remove_sink(Sink* sink) {
  std::erase(sinks, sink);
}

}  // namespace erl::logging