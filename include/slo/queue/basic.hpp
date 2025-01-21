#pragma once
#include <mutex>
#include <queue>
#include <span>
#include <stop_token>

namespace slo {
template <typename T>
struct BasicQueue {
  template <typename U>
  void push(U&& data) {
    std::scoped_lock lock{m};
    queue.push(std::forward<U>(data));
    c.notify_one();
  }

  template <typename U>
  bool try_push(U&& data) {
    std::unique_lock lock(m, std::try_to_lock);
    if(lock.owns_lock()){
      queue.emplace(std::forward<U>(data));
      c.notify_one();
      return true;
    }
    return false;
  }

  T pop(std::stop_token stop_token = {}) {
    std::unique_lock lock{m};
    c.wait(lock, [this, stop_token] { return !queue.empty() || stop_token.stop_requested(); });

    if (queue.empty()){
      // only return an empty vector if the queue was empty
      // => drain the queue before stopping
      return {};
    }
    auto val = queue.front();
    queue.pop();
    return val;
  }

  bool try_pop(T* target) {
    std::unique_lock lock(m, std::try_to_lock);
    if (!lock.owns_lock()) {
      return false;
    }

    if (queue.empty()) {
      return false;
    }

    new (target) T(queue.front());
    queue.pop();
    return true;
  }

  [[nodiscard]] bool is_empty() const {
    std::scoped_lock lock{m};
    return queue.empty();
  }

  void notify_all(){
    c.notify_all();
  }

private:
  std::queue<T> queue;
  mutable std::mutex m;
  std::condition_variable c;
};
}  // namespace slo