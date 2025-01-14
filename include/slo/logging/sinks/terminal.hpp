#pragma once
#include <cstdio>

#include <slo/threading/info.hpp>
#include "sink.hpp"
#include <print>

namespace slo::logging {
  struct Terminal : Sink {
    ~Terminal() override {}
    void print(Message const& message) override {
      auto thread_info = threading::Info(message.thread);
      auto thread_name = thread_info.get_name();
      if (thread_name.empty()){
        thread_name = "Unnamed";
      }
      std::print("[{}]{}\n", thread_name, message.text);
    }
  };
}