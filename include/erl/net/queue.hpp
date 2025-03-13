#pragma once
#include <concepts>
#include <cstddef>

namespace erl::net {
  template <typename T>
concept is_queue = requires(T obj) {
  typename T::element_type;
  { obj.push(typename T::element_type{}) } -> std::same_as<void>;
  { obj.pop() } -> std::same_as<typename T::element_type>;
};

template <typename U, typename V>
struct QueueClient {
  U* in;
  V* out;

  void send(auto const& message)
    requires(is_queue<U>)
  {
    in->push(message);
  }

  auto recv()
    requires(is_queue<V>)
  {
    return out->pop();
  }

  void kill()
    requires(is_queue<U>)
  {
    auto goodbye_message = typename U::element_type{};
    in->push(goodbye_message);
  }
};

template <is_queue U>
QueueClient(U*, std::nullptr_t) -> QueueClient<U, void>;

template <is_queue V>
QueueClient(std::nullptr_t, V*) -> QueueClient<void, V>;
}