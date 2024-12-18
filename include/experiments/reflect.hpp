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
#include "util/concepts.hpp"
#include "util/hash.hpp"

namespace rpc {

template <typename T>
concept Serializer = requires(T obj) {
  { obj.extend(nullptr, 1) } -> std::same_as<void>;
  { obj.reserve(1) } -> std::same_as<void>;
};

template <typename T>
concept Deserializer = requires(T obj) {
  { obj.template get<int>() } -> std::same_as<int>;
};

template <typename T>
struct Reflect {
  static_assert(false, "Cannot reflect T");
};
template <std::integral T>
struct Reflect<T> {
  static void serialize(T arg, Serializer auto& target) {
    if constexpr (std::endian::native == std::endian::big) {
      arg = std::byteswap(arg);
    }
    auto old_size = target.size();
    target.extend(&arg, sizeof(arg));
  }

  // TODO Deserialize concept is recursive if used here
  static T deserialize(auto& buffer) {
    std::remove_const_t<T> value;
    // TODO error handling?
    // if (!buffer.has_n(sizeof(T))) {
    //     throw 1;
    // }

    std::memcpy(&value, buffer.current(), sizeof(T));
    buffer.cursor += sizeof(T);

    if constexpr (std::endian::native == std::endian::big) {
      value = std::byteswap(value);
    }
    return value;
  }

  consteval static void hash_append(auto& hasher) { hasher(display_string_of(^T)); }
};

template <util::pair_like T>
struct Reflect<T> {
  using first_type  = typename T::first_type;
  using second_type = typename T::second_type;
  static void serialize(auto&& arg, Serializer auto& target) {
    target.add_field(arg.first);
    target.add_field(arg.second);
  }

  static T deserialize(Deserializer auto& buffer) {
    return T(buffer.template get<first_type>(), buffer.template get<second_type>());
  }

  consteval static void hash_append(auto& hasher) {
    hasher(display_string_of(^T));
    Reflect<std::remove_cvref_t<first_type>>::hash_append(hasher);
    Reflect<std::remove_cvref_t<second_type>>::hash_append(hasher);
  }
};

template <typename T>
  requires std::is_enum_v<T>
struct Reflect<T> {
  static void serialize(auto&& arg, Serializer auto& target) {
    Reflect<std::underlying_type_t<T>>::serialize(std::forward<decltype(arg)>(arg), target);
  }

  static T deserialize(Deserializer auto& buffer) { return T(Reflect<std::underlying_type_t<T>>::deserialize(buffer)); }

  consteval static void hash_append(auto& hasher) {
    hasher(display_string_of(^T));
    Reflect<std::underlying_type_t<T>>::hash_append(hasher);
  }
};

template <template <typename...> class Variant, typename... Ts>
  requires std::derived_from<Variant<Ts...>, std::variant<Ts...>>
struct Reflect<Variant<Ts...>> {
  static void serialize(auto&& arg, Serializer auto& target) {
    Reflect<std::size_t>::serialize(arg.index(), target);
    std::visit([&](auto&& alt) { Reflect<std::remove_cvref_t<decltype(alt)>>::serialize(alt, target); }, arg);
  }

  static decltype(auto) deserialize(Deserializer auto& buffer) {
    auto index = Reflect<std::size_t>::deserialize(buffer);
    return [&]<std::size_t... Idx>(std::index_sequence<Idx...>) {
      union Storage {
        char dummy{};
        Variant<Ts...> variant;
      } storage;
      (void)((Idx == index ? (std::construct_at(&storage.variant, Reflect<Ts...[Idx]>::deserialize(buffer)), true)
                           : false) ||
             ...);
      return std::move(storage.variant);
    }(std::index_sequence_for<Ts...>{});
  }

  consteval static void hash_append(auto& hasher) {
    hasher(display_string_of(^Variant<Ts...>));
    (Reflect<Ts>::hash_append(hasher), ...);
  }
};

template <typename T>
  requires(std::is_aggregate_v<T> && !std::is_array_v<T>)
struct Reflect<T> {
  static void serialize(auto&& arg, Serializer auto& target) {
    // auto[...members] = arg;
    //(Reflect<[:remove_cvref(type_of(members)):]>::serialize(arg.[:members:], target), ...);
    return [:meta::expand(nonstatic_data_members_of(^T)):] >> [&]<auto... Members>() {
      return (Reflect<[:remove_cvref(type_of(Members)):]>::serialize(arg.[:Members:], target), ...);
    };
  }

  static T deserialize(Deserializer auto& buffer) {
    return [:meta::expand(nonstatic_data_members_of(^T)):] >> [&]<auto... Members>() {
      return T{Reflect<[:remove_cvref(type_of(Members)):]>::deserialize(buffer)...};
    };
  }

  consteval static void hash_append(auto& hasher) {
    hasher(display_string_of(^T));
    // TODO hash bases
    [&]<std::size_t... Idx>(std::index_sequence<Idx...>) {
      (Reflect<[:remove_cvref(type_of(meta::nth_nsdm<T>(Idx))):]>::hash_append(hasher), ...);
    }(std::make_index_sequence<nonstatic_data_members_of(^T).size()>{});
  }
};

template <std::ranges::range T>
  requires std::constructible_from<std::initializer_list<typename T::value_type>>
struct Reflect<T> {
  using element_type = typename T::value_type;
  static void serialize(auto&& arg, Serializer auto& target) {
    std::uint32_t size = arg.size();
    target.add_field(size);
    target.reserve(sizeof(element_type) * size);
    for (auto&& element : arg) {
      target.add_field(element);
    }
  }

  static auto deserialize(Deserializer auto& buffer) {
    std::uint32_t size = buffer.template get<std::uint32_t>();
    std::vector<element_type> elements{};
    elements.reserve(size);
    for (std::uint32_t idx = 0; idx < size; ++idx) {
      elements.push_back(buffer.template get<element_type>());
    }
    if constexpr (is_template(^T) && template_of(^T) == ^std::basic_string_view) {
      // cannot serialize to non-owning view, produce string instead
      return std::string(begin(elements), end(elements));
    } else {
      return T(begin(elements), end(elements));
    }
  }

  consteval static void hash_append(auto& hasher) {
    hasher(display_string_of(^T));
    Reflect<std::remove_cvref_t<typename T::value_type>>::hash_append(hasher);
  }
};

template <typename T, std::size_t N>
struct Reflect<T[N]> {
  static void serialize(auto&& arg, Serializer auto& target) {
    target.reserve(sizeof(T) * N);
    for (auto&& element : arg) {
      target.add_field(element);
    }
  }

  static std::array<T, N> deserialize(Deserializer auto& buffer) {
    std::array<T, N> elements{};
    for (std::uint32_t idx = 0; idx < N; ++idx) {
      elements[idx] = buffer.template get<T>();
    }
    return elements;
  }

  consteval static void hash_append(auto& hasher) {
    hasher(display_string_of(^T[N]));
    Reflect<std::remove_cvref_t<T>>::hash_append(hasher);
  }
};

template <typename T, typename Hasher = util::FNV1a>
consteval auto hash_type(){
    auto hasher = Hasher{};
    Reflect<std::remove_cvref_t<T>>::hash_append(hasher);
    return hasher.finalize();
}
}  // namespace rpc