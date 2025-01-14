#pragma once
#include <mutex>
#include <queue>
#include <span>
#include <stop_token>

namespace slo::transport {
struct MessageQueue {
  void send(std::span<char> message) {
    std::scoped_lock lock{m};
    queue.emplace(message.begin(), message.end());
    c.notify_one();
  }

  std::vector<char> receive(std::stop_token const& stop_token) {
    std::unique_lock<std::mutex> lock(m);
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

  [[nodiscard]] bool is_empty() const {
    std::scoped_lock lock{m};
    return queue.empty();
  }

  void notify_all(){
    c.notify_all();
  }

private:
  std::queue<std::vector<char>> queue;
  mutable std::mutex m;
  std::condition_variable c;
};
}  // namespace slo::transport