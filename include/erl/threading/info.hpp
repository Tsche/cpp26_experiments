#pragma once
#include <cstdint>
#include <string>
#include <shared_mutex>
#include <unordered_map>

namespace erl {
struct ThreadInfo {
  std::uint64_t id;

  static ThreadInfo current() noexcept;
  void set_name(std::string const& name) const;
  [[nodiscard]] std::string get_name() const;
};

inline thread_local ThreadInfo const this_thread{ThreadInfo::current()};

namespace thread {
  struct NameCache {
    static NameCache& get_cache();
    mutable std::shared_mutex mutex;
    std::unordered_map<std::uint64_t, std::string> data;

    void cache_name(std::uint64_t id, std::string_view name);
  };

  void set_name(std::uint64_t id, std::string const& name);
  void set_name(std::string const& name);

  std::string get_name(std::uint64_t id);
  std::string get_name();
}
}  // namespace erl