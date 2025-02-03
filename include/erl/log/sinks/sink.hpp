#pragma once
#include <erl/log/message.hpp>

namespace erl::logging {
  struct Sink{
    virtual ~Sink() = default;
    virtual void print(Message const&) = 0;
  };
}