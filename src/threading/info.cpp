
#include <mutex>
#include <type_traits>
#include <unordered_map>
#include <shared_mutex>

#include <erl/log/logger.hpp>
#include <erl/thread.hpp>

#include "platform/info.h"
#include <iostream>

namespace erl {

// namespace {
// struct CachedThreadInfo {
//   std::string name;
//   std::uint64_t parent{0};
// };

// struct ThreadCache {
//   mutable std::shared_mutex data_mutex;
//   std::unordered_map<std::uint64_t, CachedThreadInfo> data;

//   static ThreadCache& get_cache() {
//     static ThreadCache instance;
//     return instance;
//   }

//   void cache_name(std::uint64_t thread, std::string_view name) {
//     std::unique_lock lock{data_mutex};
//     data[thread].name = std::string(name);
//   }

//   void set_parent(std::uint64_t thread, std::uint64_t parent) {
//     std::unique_lock lock{data_mutex};
//     data[thread].parent = parent;
//   }

//   std::uint64_t get_parent(std::uint64_t thread) {
//     std::shared_lock lock{data_mutex};
//     auto it = data.find(thread);
//     if (it != data.end()) {
//       return it->second.parent;
//     }
//     return 0;
//   }

//   void unlink(std::uint64_t thread) {
//     // std::cout << "unlinking " << id << ' ' << names[id] << ' ' << get_parent(id) << "\n";
//     {
//       std::unique_lock lock{data_mutex};
//       data.erase(thread);
//     }
//   }
// };

// template <typename F>
// struct RunOnExit {
//   F fnc;

//   template <typename T>
//     requires std::same_as<std::remove_cvref_t<T>, F>
//   RunOnExit(T&& fnc_) : fnc(fnc_) {}

//   RunOnExit(RunOnExit const&)            = delete;
//   RunOnExit& operator=(RunOnExit const&) = delete;

//   ~RunOnExit() { fnc(); }
// };

// template <typename F>
// RunOnExit(F&&) -> RunOnExit<std::remove_cvref_t<F>>;

// static void ensure_unregistration(std::uint64_t id) {
//   // unlinking early can cause other threads to fail to find this thread's name

//   thread_local auto unregistration_helper = RunOnExit{[id] {
//     auto& cache = ThreadCache::get_cache();
//     cache.unlink(id);
//   }};
// }
// }  // namespace

ThreadInfo ThreadInfo::current() noexcept {
  return {thread::impl::current()};
}

bool ThreadInfo::is_valid() const {
  return id != 0;
}

ThreadInfo const& ThreadInfo::set_name(std::string const& name) const {
  thread::impl::set_name(id, name);
  // TODO
  logging::Logger::client().rename(id, name);
  return *this;
}

std::string ThreadInfo::get_name() const {
  // auto& cache = ThreadCache::get_cache();
  // {
  //   std::shared_lock lock{cache.data_mutex};
  //   auto it = cache.data.find(id);
  //   if (it != cache.data.end()) {
  //     return it->second.name;
  //   }
  // }
  auto name = thread::impl::get_name(id);
  // cache.cache_name(id, name);
  return name;
}

ThreadInfo const& ThreadInfo::set_parent(ThreadInfo parent) const {
  // ensure_unregistration(id);
  logging::Logger::client().set_parent(id, parent.id);
  return *this;
}

ThreadInfo ThreadInfo::get_parent() const {
//   auto& cache = ThreadCache::get_cache();
//   return {cache.get_parent(id)};
  return {};
}

}  // namespace erl