#include <erl/threading/info.hpp>
#include <mutex>

#include "platform/info.h"

namespace erl::thread {

NameCache& NameCache::get_cache(){
  static NameCache instance;
  return instance;
}

void NameCache::cache_name(std::uint64_t id, std::string_view name){
  std::unique_lock lock{mutex};
  data[id] = std::string(name);
  
}

void set_name(std::uint64_t id, std::string const& name){
  auto& cache = NameCache::get_cache();
  impl::set_name(id, name);
  cache.cache_name(id, name);
}

void set_name(std::string const& name){
  set_name(impl::current(), name);
}

std::string get_name(std::uint64_t id){
  auto& cache = NameCache::get_cache();
  {
    std::shared_lock lock{cache.mutex};
    auto it = cache.data.find(id);
    if (it != cache.data.end()){
      return it->second;
    }
  }
  auto name = impl::get_name(id);
  cache.cache_name(id, name);
  return name;
}

std::string get_name(){
  return get_name(impl::current());
}
}