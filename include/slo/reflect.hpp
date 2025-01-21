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

namespace slo {

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
std::remove_cvref_t<T> deserialize(Deserializer auto& buffer) {
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

namespace impl {
template <typename T>
concept pair_like = requires(T obj) {
  obj.first;
  obj.second;
  requires std::is_same_v<typename T::first_type, decltype(obj.first)>;
  requires std::is_same_v<typename T::second_type, decltype(obj.second)>;
};
}

template <impl::pair_like T>
struct Reflect<T> {
  using first_type  = typename T::first_type;
  using second_type = typename T::second_type;
  static void serialize(auto&& arg, Serializer auto& target) {
    slo::serialize(arg.first, target);
    slo::serialize(arg.second, target);
  }

  static T deserialize(Deserializer auto& buffer) {
    return T(slo::deserialize<first_type>(buffer), slo::deserialize<second_type>(buffer));
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
    slo::serialize<std::underlying_type_t<T>>(std::forward<decltype(arg)>(arg), target);
  }

  static T deserialize(Deserializer auto& buffer) { 
    return T(slo::deserialize<std::underlying_type_t<T>>(buffer)); }

  consteval static void hash_append(auto& hasher) {
    hasher(display_string_of(^^T));
    Reflect<std::underlying_type_t<T>>::hash_append(hasher);
  }
};

template <template <typename...> class Variant, typename... Ts>
  requires std::derived_from<Variant<Ts...>, std::variant<Ts...>>
struct Reflect<Variant<Ts...>> {
  static void serialize(auto&& arg, Serializer auto& target) {
    slo::serialize(arg.index(), target);
    std::visit([&](auto&& alt) { 
      slo::serialize(alt, target); 
    }, arg);
  }

  static decltype(auto) deserialize(Deserializer auto& buffer) {
    auto index = slo::deserialize<std::size_t>(buffer);
    return [&]<std::size_t... Idx>(std::index_sequence<Idx...>) {
      union Storage {
        char dummy{};
        Variant<Ts...> variant;
      } storage;
      (void)((Idx == index ? (std::construct_at(&storage.variant, slo::deserialize<Ts...[Idx]>(buffer)), true)
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
    //(Reflect<[:remove_cvref(type_of(members)):]>::serialize(arg.[:members:], target), ...);
    return [:meta::expand(nonstatic_data_members_of(^^T)):] >> [&]<auto... Members>() {
      return (slo::serialize(arg.[:Members:], target), ...);
    };
  }

  static T deserialize(Deserializer auto& buffer) {
    return [:meta::expand(nonstatic_data_members_of(^^T)):] >> [&]<auto... Members>() {
      return T{slo::deserialize<[:remove_cvref(type_of(Members)):]>(buffer)...};
    };
  }

  consteval static void hash_append(auto& hasher) {
    hasher(display_string_of(^^T));
    // TODO hash bases
    [&]<std::size_t... Idx>(std::index_sequence<Idx...>) {
      (Reflect<[:remove_cvref(type_of(meta::nth_nsdm<T>(Idx))):]>::hash_append(hasher), ...);
    }(std::make_index_sequence<nonstatic_data_members_of(^^T).size()>{});
  }
};

template <std::ranges::range T>
  requires std::constructible_from<std::initializer_list<typename T::value_type>>
struct Reflect<T> {
  using element_type = typename T::value_type;
  static void serialize(auto&& arg, Serializer auto& target) {
    std::uint32_t size = arg.size();
    slo::serialize(size, target);
    target.reserve(sizeof(element_type) * size);
    for (auto&& element : arg) {
      slo::serialize(element, target);
    }
  }

  static auto deserialize(Deserializer auto& buffer) {
    std::uint32_t size = slo::deserialize<std::uint32_t>(buffer);
    std::vector<element_type> elements{};
    elements.reserve(size);
    for (std::uint32_t idx = 0; idx < size; ++idx) {
      elements.push_back(slo::deserialize<element_type>(buffer));
    }
    if constexpr (is_template(^^T) && template_of(^^T) == ^^std::basic_string_view) {
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
      slo::serialize(element, target);
    }
  }

  static std::array<T, N> deserialize(Deserializer auto& buffer) {
    std::array<T, N> elements{};
    for (std::uint32_t idx = 0; idx < N; ++idx) {
      elements[idx] = slo::deserialize<T>(buffer);
    }
    return elements;
  }

  consteval static void hash_append(auto& hasher) {
    hasher(display_string_of(^^T[N]));
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