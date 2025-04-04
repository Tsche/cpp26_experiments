#include <chrono>

#include <erl/_impl/log/logger.hpp>
#include <erl/_impl/log/message.hpp>
#include <erl/_impl/net/message/reader.hpp>
#include <erl/thread.hpp>
#include <erl/clock.hpp>


namespace erl::logging {
namespace _impl {
ThreadEventHelper::ThreadEventHelper() noexcept {
  Logger::client().spawn(current_time(), this_thread.id);
}
ThreadEventHelper::~ThreadEventHelper() noexcept {
  Logger::client().exit(current_time(), this_thread.id);
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

LoggingEvent LoggingService::make_prelude(erl::logging::Severity severity, erl::logging::formatter_type formatter) {
  return {.severity = severity, .thread = erl::this_thread, .timestamp = current_time(), .handler = formatter};
}

void LoggingService::handle_print(std::span<char const> data) {
  auto reader           = erl::message::MessageView{data};
  auto event            = deserialize<LoggingEvent>(reader);
  auto [location, text] = event.handler(reader.buffer.subspan(reader.cursor));
  auto message          = Message{.severity  = event.severity,
                                  .thread    = thread_get_info(event.thread.id),
                                  .timestamp = to_timepoint(event.timestamp),
                                  .location  = location,
                                  .text      = text};
  dispatch(&Sink::print, message);
}

void LoggingService::spawn(timestamp_t timestamp, std::uint64_t thread) {
  auto& info = threads[thread];
  info.id    = thread;
  dispatch(&Sink::spawn, to_timepoint(timestamp), info);
}

void LoggingService::exit(timestamp_t timestamp, std::uint64_t thread) {
  if (auto it = threads.find(thread); it != threads.end()) {
    dispatch(&Sink::exit, to_timepoint(timestamp), it->second);
  } else {
    dispatch(&Sink::exit, to_timepoint(timestamp), CachedThreadInfo{.id = thread});
  }

  threads.erase(thread);
}

void LoggingService::rename(timestamp_t timestamp, std::uint64_t thread, std::string_view name) {
  auto& info = threads[thread];
  dispatch(&Sink::rename, to_timepoint(timestamp), info, name);
  info.name = name;
}

void LoggingService::set_parent(timestamp_t timestamp, std::uint64_t thread, std::uint64_t parent_id) {
  auto& info   = threads[thread];
  auto& parent = threads[parent_id];
  if (parent.id != parent_id) {
    // parent just created
    parent.id = parent_id;
  }

  dispatch(&Sink::set_parent, to_timepoint(timestamp), info, parent);
  info.parent = parent_id;
}

void LoggingService::add_sink(Sink* sink) {
  sinks.push_back(sink);
}

void LoggingService::remove_sink(Sink* sink) {
  std::erase(sinks, sink);
}

}  // namespace erl::logging