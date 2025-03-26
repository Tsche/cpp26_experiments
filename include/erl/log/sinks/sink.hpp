#pragma once
#include <erl/log/message.hpp>

namespace erl::logging {
struct Sink {
  virtual ~Sink()                    = default;
  virtual void print(Message const&) = 0;

  virtual void spawn(timepoint_t timestamp, CachedThreadInfo const& /*thread*/) {}
  virtual void exit(timepoint_t timestamp, CachedThreadInfo const& /*thread*/) {}
  virtual void rename(timepoint_t timestamp, CachedThreadInfo const& /*thread*/, std::string_view /*name*/) {}
  virtual void set_parent(timepoint_t timestamp, CachedThreadInfo const& /*thread*/, CachedThreadInfo const& /*parent*/) {}
};

struct NullSink final : Sink {
  ~NullSink() override = default;
  void print(Message const&) override {}
};
}  // namespace erl::logging