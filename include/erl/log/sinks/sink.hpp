#pragma once
#include <erl/log/message.hpp>

namespace erl::logging {
struct Sink {
  virtual ~Sink()                    = default;
  virtual void print(Message const&) = 0;

  virtual void spawn(CachedThreadInfo const& /*thread*/) {}
  virtual void exit(CachedThreadInfo const& /*thread*/) {}
  virtual void rename(CachedThreadInfo const& /*info*/, std::string_view /*name*/) {}
  virtual void set_parent(CachedThreadInfo const& /*info*/, CachedThreadInfo const& /*parent*/) {}
};

struct NullSink final : Sink {
  ~NullSink() override = default;
  void print(Message const&) override {}
};
}  // namespace erl::logging