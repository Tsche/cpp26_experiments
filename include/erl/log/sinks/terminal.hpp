#pragma once
#include <cstdio>
#include <format>
#include <print>
#include <filesystem>
#include <erl/thread.hpp>

#include <erl/log/message.hpp>
#include <erl/log/format/log.hpp>
#include <erl/log/format/color.hpp>

#include "sink.hpp"

namespace erl::logging {
struct Terminal : Sink {
  LogFormat formatter;
  Severity minimum_severity;
  std::unordered_map<Severity, std::string_view> color_map;

  ~Terminal() override = default;
  explicit Terminal(LogFormat formatter,
                    Severity min_severity = Severity::DEBUG,
                    std::unordered_map<Severity, std::string_view> color_map = {})
      : formatter(formatter)
      , minimum_severity(min_severity)
      , color_map(color_map) {}

  void spawn(CachedThreadInfo const& thread) override {
    print({.severity = Severity::DEBUG, .thread = thread, .text = std::format("thread {} started", thread.id)});
  }

  void exit(CachedThreadInfo const& thread) override {
    print({.severity = Severity::DEBUG,
           .thread   = thread,
           .text     = std::format("thread {} `{}` exited", thread.id, thread.name)});
  }

  void rename(CachedThreadInfo const& info, std::string_view name) override {
    print({.severity = Severity::DEBUG,
           .thread   = info,
           .text     = std::format("thread {} `{}` renamed to `{}`", info.id, info.name, name)});
  }

  void set_parent(CachedThreadInfo const& info, CachedThreadInfo const& parent) override {
    print({.severity = Severity::DEBUG,
           .thread   = info,
           .text = std::format("thread {} `{}` parent set to {} `{}`", info.id, info.name, parent.id, parent.name)});
  }

  void print(Message const& message) override {
    if (message.severity < minimum_severity) {
      return;
    }

    std::string_view color = "";
    if (auto it = color_map.find(message.severity); it != color_map.end()) {
      color = it->second;
    }

    std::puts(formatter(message, color).c_str());
  }
};
}  // namespace erl::logging