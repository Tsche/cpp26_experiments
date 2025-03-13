#pragma once
#include <erl/queue/spsc_bounded.hpp>
#include <erl/queue/mpmc_bounded.hpp>
#include <erl/net/queue.hpp>
#include <erl/net/service.hpp>
#include <erl/rpc/protocol.hpp>
#include <erl/rpc/proxy.hpp>


namespace erl {
namespace rpc {

using namespace erl::rpc::annotations;
using namespace erl::rpc::policies;
}


template <typename Message>
struct Pipe {
  using message_queue = erl::queues::BoundedSPSC<Message, 64>;

  message_queue in{};
  message_queue out{};

  auto make_server() { return net::Server{rpc::BlockingCall{net::QueueClient{&out, &in}}}; }
  auto make_client() { return rpc::BlockingCall{net::QueueClient{&in, &out}}; }
};

template <typename Message>
struct EventQueue {
  using message_queue = erl::queues::BoundedMPMC<Message, 64>;

  message_queue events{};

  auto make_server() { return net::Server{rpc::EventCall{net::QueueClient{nullptr, &events}}}; }
  auto make_client() { return rpc::EventCall{net::QueueClient{&events, nullptr}}; }
};

}