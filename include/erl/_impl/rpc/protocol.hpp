#pragma once
#include <cstdint>
#include <experimental/meta>
#include <span>

#include <erl/_impl/rpc/proxy.hpp>
#include <erl/reflect.hpp>
#include <erl/_impl/net/message/reader.hpp>

#include <print>

namespace erl::rpc {
template <typename Client>
struct BlockingCall : Client {
  template <typename Service, typename R, typename... Args>
  auto call(std::size_t index, Args&&... args) {
    using protocol = typename Service::protocol;

    auto request = protocol::request(index, std::forward<Args>(args)...);
    Client::send(request);
    auto response = Client::recv();
    return protocol::template read_response<R>(index, std::span<char const>{response});
  }

  template <typename Service>
  void handle(Service&& service, std::span<char const> message) {
    using protocol = typename std::remove_cvref_t<Service>::protocol;

    auto reply = protocol::dispatch(std::forward<Service>(service), std::span<char const>{message});
    Client::send(reply);
  }
};


template <typename Client>
struct EventCall : Client {
  template <typename Service, typename R, typename... Args>
  void call(std::size_t index, Args&&... args) {
    using protocol = typename Service::protocol;

    auto request = protocol::request(index, std::forward<Args>(args)...);
    Client::send(request);
  }

  template <typename Service>
  void handle(Service&& service, std::span<char const> message) {
    using protocol = typename std::remove_cvref_t<Service>::protocol;

    protocol::dispatch(std::forward<Service>(service), std::span<char const>{message});
  }
};

template <typename MsgType>
struct RPCProtocol {
  using message_type = MsgType;
  using index_type   = std::uint32_t;

  template <typename... Args>
  static message_type request(index_type index, Args&&... args) {
    auto message = message_type{};
    erl::serialize(index, message);
    (erl::serialize(std::forward<Args>(args), message), ...);
    return message;
  }

  template <typename... Args>
  static message_type request(index_type index, message_type payload) {
    auto message = message_type{};
    erl::serialize(index, message);
    message.reserve(payload.size());
    message.write(payload.finalize());
    return message;
  }

  template <typename S>
  static message_type dispatch(S&& service, std::span<char const> message) {
    auto reader                      = erl::message::MessageView{message};
    auto index                       = erl::deserialize<index_type>(reader);
    auto remainder                   = reader.buffer.subspan(reader.cursor);
    constexpr static auto dispatcher = erl::rpc::Dispatcher<S, RPCProtocol>{};
    return dispatcher(std::forward<S>(service), index, remainder);
  }

  template <typename... Ts>
  static message_type make_response(index_type index, Ts&&... value) {
    auto message = message_type{};
    erl::serialize(index, message);
    (erl::serialize(std::forward<Ts>(value), message), ...);
    return message;
  }

  template <typename T>
  static T read_response(index_type expected_index, std::span<char const> message) {
    auto reader = erl::message::MessageView{message};
    auto index  = erl::deserialize<index_type>(reader);
    assert(index == expected_index);
    if constexpr (!std::same_as<T, void>) {
      return erl::deserialize<T>(reader);
    }
  }
};
}