#pragma once
#include <print>
#include <filesystem>
#include <erl/threading/info.hpp>

#include "sink.hpp"

#define RESET   "\033[0m"
#define BLACK   "\033[30m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define BOLD    "\033[1m"

namespace erl::logging {
struct Terminal : Sink {
  ~Terminal() override = default;

  static constexpr std::string_view fmt =
      "[{}][" GREEN "{}" RESET "][" BLUE "{}" RESET "][" GREEN "{}:{}:{}" RESET "]: {}\n";

  void print(Message const& message) override {
    auto thread_name = erl::thread::get_name(message.meta.thread.id);
    // if (thread_name.empty()) {
      // thread_name = "Unnamed";
    // }
    // auto file_name = std::filesystem::path(message.location.file).filename().string();
    auto file_name = message.location.file;
    std::print(fmt, message.meta.timestamp, thread_name, message.location.function, file_name, message.location.line,
               message.location.column, message.text);
  }
};
}  // namespace erl::logging