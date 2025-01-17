#pragma once
#include <slo/log/message.hpp>

namespace slo::logging {
  struct Sink{
    virtual ~Sink() = default;
    virtual void print(Message const&) = 0;
  };
}