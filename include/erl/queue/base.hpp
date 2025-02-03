#pragma once
#include <thread>
#include <type_traits>
#include <stop_token>

namespace erl::queues::impl {

struct QueueBase {
  template <typename T>
  auto pop(this T&& self, std::stop_token const& token) -> std::remove_cvref_t<T>::element_type {
    typename std::remove_cvref_t<T>::element_type obj;
    
    while(!std::forward<T>(self).try_pop(&obj)){
      if (token.stop_requested()){
        return {};
      }
      std::this_thread::yield();
    }
    return obj;
  }

  template <typename T, typename U>
  void push(this T&& self, U&& obj){
    while (!std::forward<T>(self).try_push(std::forward<U>(obj))) {
      std::this_thread::yield();
    }
  }
};
}