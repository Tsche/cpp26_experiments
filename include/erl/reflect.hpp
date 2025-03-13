#pragma once
#include <bit>
#include <concepts>
#include <cstring>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <ranges>
#include <variant>
#include <vector>

#include <experimental/meta>
#include "util/meta.hpp"
#include "util/hash.hpp"

namespace erl {

template <typename T>
concept Serializer = requires(T obj) {
  { obj.write(nullptr, 1) } -> std::same_as<void>;
  { obj.reserve(1) } -> std::same_as<void>;
};

template <typename T>
concept Deserializer = requires(T obj) {
  { obj.read(1) } -> std::same_as<std::span<char const>>;
};

template <typename T>
struct Reflect {
  static_assert(false, "Cannot reflect T");
};

template <typename T>
void serialize(T&& obj, Serializer auto& buffer) {
  Reflect<std::remove_cvref_t<T>>::serialize(std::forward<T>(obj), buffer);
}

template <typename T>
auto deserialize(Deserializer auto& buffer) {
  return Reflect<std::remove_cvref_t<T>>::deserialize(buffer);
}

template <std::integral T>
struct Reflect<T> {
  static void serialize(T arg, Serializer auto& buffer) {
    if constexpr (std::endian::native == std::endian::big) {
      arg = std::byteswap(arg);
    }
    buffer.write(&arg, sizeof(arg));
  }

  static T deserialize(Deserializer auto& buffer) {
    std::remove_const_t<T> value;

    auto raw = buffer.read(sizeof(T));
    std::memcpy(&value, raw.data(), sizeof(T));

    if constexpr (std::endian::native == std::endian::big) {
      value = std::byteswap(value);
    }
    return value;
  }

  consteval static void hash_append(auto& hasher) { hasher(display_string_of(^^T)); }
};

// TODO floats

namespace impl {
template <typename T>
concept pair_like = requires(T obj) {
  obj.first;
  obj.second;
  requires std::is_same_v<typename T::first_type, decltype(obj.first)>;
  requires std::is_same_v<typename T::second_type, decltype(obj.second)>;
};
}  // namespace impl

template <impl::pair_like T>
struct Reflect<T> {
  using first_type  = typename T::first_type;
  using second_type = typename T::second_type;
  static void serialize(auto&& arg, Serializer auto& target) {
    erl::serialize(arg.first, target);
    erl::serialize(arg.second, target);
  }

  static T deserialize(Deserializer auto& buffer) {
    auto first  = erl::deserialize<first_type>(buffer);
    auto second = erl::deserialize<second_type>(buffer);
    return T(first, second);
  }

  consteval static void hash_append(auto& hasher) {
    hasher(display_string_of(^^T));
    Reflect<std::remove_cvref_t<first_type>>::hash_append(hasher);
    Reflect<std::remove_cvref_t<second_type>>::hash_append(hasher);
  }
};

template <typename T>
  requires std::is_enum_v<T>
struct Reflect<T> {
  static void serialize(auto&& arg, Serializer auto& target) {
    erl::serialize<std::underlying_type_t<T>>(static_cast<std::underlying_type_t<T>>(std::forward<decltype(arg)>(arg)),
                                              target);
  }

  static T deserialize(Deserializer auto& buffer) { return T(erl::deserialize<std::underlying_type_t<T>>(buffer)); }

  consteval static void hash_append(auto& hasher) {
    hasher(display_string_of(^^T));
    Reflect<std::underlying_type_t<T>>::hash_append(hasher);
  }
};

template <template <typename...> class Variant, typename... Ts>
  requires std::derived_from<Variant<Ts...>, std::variant<Ts...>>
struct Reflect<Variant<Ts...>> {
  static void serialize(auto&& arg, Serializer auto& target) {
    erl::serialize(arg.index(), target);
    std::visit([&](auto&& alt) { erl::serialize(alt, target); }, arg);
  }

  static decltype(auto) deserialize(Deserializer auto& buffer) {
    auto index = erl::deserialize<std::size_t>(buffer);
    return [&]<std::size_t... Idx>(std::index_sequence<Idx...>) {
      // TODO change approach - this fails for move only alternatives etc
      union Storage {
        char dummy{};
        Variant<Ts...> variant;
      } storage;
      (void)((Idx == index ? (std::construct_at(&storage.variant, erl::deserialize<Ts...[Idx]>(buffer)), true)
                           : false) ||
             ...);
      return std::move(storage.variant);
    }(std::index_sequence_for<Ts...>{});
  }

  consteval static void hash_append(auto& hasher) {
    hasher(display_string_of(^^Variant<Ts...>));
    (Reflect<Ts>::hash_append(hasher), ...);
  }
};

template <typename T>
  requires(std::is_aggregate_v<T> && !std::is_array_v<T>)
struct Reflect<T> {
  static void serialize(auto&& arg, Serializer auto& target) {
    // auto[...members] = arg;
    //(Reflect<std::remove_cvref_t<decltype(members)>>::serialize(members, target), ...);
    return [:meta::expand(nonstatic_data_members_of(^^T)):] >> [&]<auto... Members>() {
      return (erl::serialize(arg.[:Members:], target), ...);
    };
  }

  static T deserialize(Deserializer auto& buffer) {
    return [:meta::expand(nonstatic_data_members_of(^^T)):] >> [&]<auto... Members>() {
      return T{erl::deserialize<[:remove_cvref(type_of(Members)):]>(buffer)...};
    };
  }

  consteval static void hash_append(auto& hasher) {
    hasher(display_string_of(^^T));
    // TODO hash bases
    [&]<std::size_t... Idx>(std::index_sequence<Idx...>) {
      (Reflect<[:remove_cvref(type_of(meta::get_nth_member(^^T, Idx))):]>::hash_append(hasher), ...);
    }(std::make_index_sequence<nonstatic_data_members_of(^^T).size()>{});
  }
};

template <std::ranges::range T>
  requires std::constructible_from<std::initializer_list<typename T::value_type>>
struct Reflect<T> {
  using element_type = typename T::value_type;
  static void serialize(auto&& arg, Serializer auto& target) {
    std::uint32_t size = arg.size();
    erl::serialize(size, target);
    target.reserve(sizeof(element_type) * size);
    for (auto&& element : arg) {
      erl::serialize(element, target);
    }
  }

  static auto deserialize(Deserializer auto& buffer) {
    // TODO reject deserialization to non-owning views, force conversion in calling code

    std::uint32_t size = erl::deserialize<std::uint32_t>(buffer);
    std::vector<element_type> elements{};
    elements.reserve(size);
    for (std::uint32_t idx = 0; idx < size; ++idx) {
      elements.push_back(erl::deserialize<element_type>(buffer));
    }
    if constexpr (std::same_as<T, std::string_view>) {
      // cannot serialize to non-owning view, produce string instead
      return std::string(begin(elements), end(elements));
    } else {
      return T(begin(elements), end(elements));
    }
  }

  consteval static void hash_append(auto& hasher) {
    hasher(display_string_of(^^T));
    Reflect<std::remove_cvref_t<typename T::value_type>>::hash_append(hasher);
  }
};

template <typename T, std::size_t N>
struct Reflect<T[N]> {
  static void serialize(auto&& arg, Serializer auto& target) {
    target.reserve(sizeof(T) * N);
    for (auto&& element : arg) {
      erl::serialize(element, target);
    }
  }

  static std::array<T, N> deserialize(Deserializer auto& buffer) {
    std::array<T, N> elements{};
    for (std::uint32_t idx = 0; idx < N; ++idx) {
      elements[idx] = erl::deserialize<T>(buffer);
    }
    return elements;
  }

  consteval static void hash_append(auto& hasher) {
    hasher(display_string_of(^^T[N]));
    Reflect<std::remove_cvref_t<T>>::hash_append(hasher);
  }
};

template <typename R, typename... Args>
struct Reflect<R (*)(Args...)> {
  // TODO clarify that this only works with in-process transports
  using type = R (*)(Args...);

  static void serialize(type arg, Serializer auto& target) {
    auto value = std::uintptr_t(arg);
    erl::serialize(value, target);
  }

  static type deserialize(Deserializer auto& buffer) {
    auto value = erl::deserialize<std::uintptr_t>(buffer);
    return reinterpret_cast<type>(value);
  }

  consteval static void hash_append(auto& hasher) {
    hasher(display_string_of(^^R (*)(Args...)));
    Reflect<std::remove_cvref_t<R>>::hash_append(hasher);
    (Reflect<std::remove_cvref_t<Args>>::hash_append(hasher), ...);
  }
};

template <typename T>
  requires(!std::same_as<std::remove_const_t<T>, char>)
struct Reflect<T*> {
  // TODO clarify that this only works with in-process transports
  using type = T*;

  static void serialize(type arg, Serializer auto& target) {
    auto value = std::uintptr_t(arg);
    erl::serialize(value, target);
  }

  static type deserialize(Deserializer auto& buffer) {
    auto value = erl::deserialize<std::uintptr_t>(buffer);
    return reinterpret_cast<type>(value);
  }

  consteval static void hash_append(auto& hasher) {
    hasher(display_string_of(^^T*));
    Reflect<std::remove_const_t<T>>::hash_append(hasher);
  }
};

template <typename T, typename Hasher = util::FNV1a>
consteval auto hash_type() {
  auto hasher = Hasher{};
  Reflect<std::remove_cvref_t<T>>::hash_append(hasher);
  return hasher.finalize();
}
}  // namespace erl