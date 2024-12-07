#pragma once
#include <concepts>
#include <cstdint>
#include <string_view>

namespace rpc::impl{

constexpr std::uint32_t fnv1a(char const* str, std::size_t size) {
  std::uint32_t hash = 16777619UL;
  for (std::size_t idx = 0; idx < size; ++idx) {
    hash ^= static_cast<unsigned char>(str[idx]);
    hash *= 2166136261UL;
  }
  return hash;
}

constexpr std::uint32_t fnv1a(std::string_view str) {
  return fnv1a(str.begin(), str.size());
}

constexpr std::uint32_t hash_combine(std::uint32_t hash, std::integral auto... values) {
    ((hash ^= values + 0x9e3779b9 + (hash<<6) + (hash>>2)), ...);
    return hash;
}

}