#pragma once
#include <concepts>
#include <type_traits>
#include <initializer_list>
#include <ranges>
#include <experimental/meta>
#include <cstdint>


#include "message.hpp"
#include "util/concepts.hpp"
#include "util/meta.hpp"

namespace rpc {
template <typename T>
struct Reflect{
    static_assert(false, "Cannot reflect T");
};
template <std::integral T>
struct Reflect<T> {
    static void serialize(T arg, Message& target) {
        if constexpr (std::endian::native == std::endian::big) {
            arg = std::byteswap(arg);
        }
        auto old_size = target.size();
        target.extend(&arg, sizeof(arg));
    }

    template <std::size_t Extent = std::dynamic_extent>
    static T deserialize(MessageParser<Extent>& buffer) {
        std::remove_const_t<T> value;
        // if (!buffer.has_n(sizeof(T))) {
        //     throw 1;
        // }

        memcpy(&value, buffer.current(), sizeof(T));
        buffer.cursor += sizeof(T);

        if constexpr (std::endian::native == std::endian::big) {
            value = std::byteswap(value);
        }
        return value;
    }
};

template <typename T>
    requires(std::is_aggregate_v<T> && !std::is_array_v<T>)
struct Reflect<T> {
    static void serialize(auto&& arg, Message& target) {
        // auto[...members] = arg;
        //(Reflect<[:remove_cvref(type_of(members)):]>::serialize(arg.[:members:], target), ...);

        [&]<std::size_t... Idx>(std::index_sequence<Idx...>) {
            (Reflect<[:remove_cvref(type_of(meta::nth_nsdm<T>(Idx))):]>::serialize(
                 arg.[:meta::nth_nsdm<T>(Idx):], target),
             ...);
        }(std::make_index_sequence<nonstatic_data_members_of(^T).size()>{});
    }

    template <std::size_t Extent = std::dynamic_extent>
    static T deserialize(MessageParser<Extent>& buffer) {
        return [&]<std::size_t... Idx>(std::index_sequence<Idx...>) {
            return T{
                Reflect<[:remove_cvref(type_of(meta::nth_nsdm<T>(Idx))):]>::deserialize(buffer)...};
        }(std::make_index_sequence<nonstatic_data_members_of(^T).size()>{});
    }
};

template <std::ranges::range T>
    requires std::constructible_from<std::initializer_list<typename T::value_type>>
struct Reflect<T> {
    using element_type = typename T::value_type;
    static void serialize(auto&& arg, Message& target) {
        std::uint32_t size = arg.size();
        target.add_field(size);
        target.reserve(sizeof(element_type) * size);
        for (auto&& element : arg){
            target.add_field(element);
        }
    }

    template <std::size_t Extent = std::dynamic_extent>
    static auto deserialize(MessageParser<Extent>& buffer) {
        std::uint32_t size = buffer.template get_field<std::uint32_t>();
        std::vector<element_type> elements{};
        elements.reserve(size);
        for (std::uint32_t idx = 0; idx < size; ++idx){
            elements.push_back(buffer.template get_field<element_type>());
        }
        if constexpr (is_template(^T) && template_of(^T) == ^std::basic_string_view){
            // cannot serialize to non-owning view, produce string instead
            return std::string(begin(elements), end(elements));
        } else {
            return T(begin(elements), end(elements));
        }
    }
};

template <typename T, std::size_t N>
struct Reflect<T[N]> {
    static void serialize(auto&& arg, Message& target) {
        target.reserve(sizeof(T) * N);
        for (auto&& element : arg){
            target.add_field(element);
        }
    }

    template <std::size_t Extent = std::dynamic_extent>
    static std::array<T, N> deserialize(MessageParser<Extent>& buffer) {
        std::array<T, N> elements{};
        for (std::uint32_t idx = 0; idx < N; ++idx){
            elements[idx] = buffer.template get_field<T>();
        }
        return elements;
    }
};

namespace util{
template <typename T>
concept pair_like = requires(T t) {
  t.first;
  t.second;
  requires std::is_same_v<typename T::first_type, decltype(t.first)>;
  requires std::is_same_v<typename T::second_type, decltype(t.second)>;
};
}

template <util::pair_like T>
struct Reflect<T> {
    using first_type = typename T::first_type;
    using second_type = typename T::second_type;
    static void serialize(auto&& arg, Message& target) {
        target.add_field(arg.first);
        target.add_field(arg.second);
    }

    template <std::size_t Extent = std::dynamic_extent>
    static T deserialize(MessageParser<Extent>& buffer) {
        return T(buffer.template get_field<first_type>(), buffer.template get_field<second_type>());
    }
};

template <typename T>
  requires std::is_enum_v<T>
struct Reflect<T>{
    static void serialize(auto&& arg, Message& target) {
     Reflect<std::underlying_type_t<T>>::serialize(std::forward<decltype(arg)>(arg), target);
    }

    template <std::size_t Extent = std::dynamic_extent>
    static T deserialize(MessageParser<Extent>& buffer) {
        return T(Reflect<std::underlying_type_t<T>>::deserialize(buffer));
    } 
};

template <template <typename...> class Variant, typename... Ts>
  requires std::derived_from<Variant<Ts...>, std::variant<Ts...>>
struct Reflect<Variant<Ts...>> {
    static void serialize(auto&& arg, Message& target) {
        Reflect<std::size_t>::serialize(arg.index(), target);
        std::visit([&](auto&& alt){
            Reflect<std::remove_cvref_t<decltype(alt)>>::serialize(alt, target);
        }, arg);
    }

    template <std::size_t Extent = std::dynamic_extent>
    static decltype(auto) deserialize(MessageParser<Extent>& buffer) {
        auto index = Reflect<std::size_t>::deserialize(buffer);
        return [&]<std::size_t... Idx>(std::index_sequence<Idx...>){
            union Storage {
                char dummy{};
                Variant<Ts...> variant;
            } storage;
            (void)((Idx == index?(std::construct_at(&storage.variant, Reflect<Ts...[Idx]>::deserialize(buffer)), true) : false) || ...);
            return std::move(storage.variant);
        }(std::index_sequence_for<Ts...>{});
    } 
};
}