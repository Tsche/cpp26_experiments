#pragma once
#include <cstddef>
#include <type_traits>
#include <vector>
#include <concepts>

#include <experimental/meta>

#include <erl/reflect.hpp>
#include <erl/util/meta.hpp>
#include <erl/util/stamp.hpp>
#include <erl/net/message/buffer.hpp>
#include "erl/net/message/reader.hpp"

namespace erl::rpc {
namespace impl {

template <std::meta::info Meta, int, typename, typename = [:type_of(Meta):]>
struct CallProxy;

template <std::meta::info Meta, int Idx, typename Super, typename R, typename... Args>
struct CallProxy<Meta, Idx, Super, R(Args...)> {
  template <typename... Ts>
  decltype(auto) operator()(Ts&&... args) const {
    // get a pointer to Proxy superobject from this subobject
    constexpr static auto current_member = meta::get_nth_member(^^Super, Idx + 1);
    Super* that = reinterpret_cast<Super*>(std::uintptr_t(this) - offset_of(current_member).bytes);

    // first (unnamed) member of Proxy is a pointer to the actual handler
    auto* handler = that->[:meta::get_nth_member(^^Super, 0):];

    return handler->template call<R>(Idx, std::forward<Ts>(args)...);
  }

  static constexpr auto index = Idx;
  using return_type           = R;

  template <typename Obj>
    requires(parent_of(Meta) == remove_cvref(^^Obj))
  static constexpr R eval(Obj&& obj, Deserializer auto& args) {
    return (std::forward<Obj>(obj).[:Meta:])(deserialize<Args>(args)...);
  }

  template <typename Obj>
  static constexpr auto dispatch(Obj&& obj, std::span<char const> data) {
    using protocol = [:remove_pointer(type_of(meta::get_nth_member(^^Super, 0))):];
    
    auto args = message::MessageView{data};
    if constexpr (std::same_as<R, void>){
      eval(std::forward<Obj>(obj), args);
      return protocol::make_response(Idx);
    } else {
      return protocol::make_response(Idx, eval(std::forward<Obj>(obj), args));
    }
  }
};

template <std::meta::info Meta, int Idx, typename Super, typename R, typename... Args>
struct CallProxy<Meta, Idx, Super, R(Args...) const> : CallProxy<Meta, Idx, Super, R(Args...)> {};

template <typename T, typename U>
consteval auto make_proxy_t() {
  struct Proxy;
  consteval {
    std::vector args = {data_member_spec(^^U*)};
    int index        = 0;

    for (auto member_fnc : meta::member_functions_of(^^T)) {
      auto type = substitute(^^CallProxy, {reflect_value(member_fnc), std::meta::reflect_value(index++), ^^Proxy});
      args.push_back(data_member_spec(type, {.name = identifier_of(member_fnc)}));
    }

    define_aggregate(^^Proxy, args);
  }
  static_assert(is_type(^^Proxy), "Could not inject RPC proxy class");
  return ^^Proxy;
}



#define $generate_case(Idx)                                   \
  case (Idx):                                                 \
    if constexpr ((Idx) < sizeof...(Members) - 1) {           \
      using call_type = Members...[(Idx) + 1];                \
      return call_type::dispatch(std::forward<T>(obj), args); \
    }                                                         \
    std::unreachable();

#define $generate_strategy(Idx, Stamper)                                                     \
  template <>                                                                                \
  struct DispatchStrategy<(Idx)> {                                                           \
    template <typename T, typename... Members>                                               \
    static constexpr auto dispatch(T&& obj, std::size_t index, std::span<char const> args) { \
      switch (index) { Stamper(0, $generate_case); }                                         \
      std::unreachable();                                                                    \
    }                                                                                        \
  }

template <int>
struct DispatchStrategy;
$generate_strategy(1, $stamp(4));    // 4^1 potential states
$generate_strategy(2, $stamp(16));   // 4^2 potential states
$generate_strategy(3, $stamp(64));   // 4^3 potential states
$generate_strategy(4, $stamp(256));  // 4^4 potential states

#undef $generate_strategy
#undef $generate_case

// template <>
// struct DispatchStrategy<0> {
//   // fallback
//   template <typename T, typename... Members>
//   static constexpr Buffer dispatch(T&& obj, Buffer msg) {
//     using dispatch_type = Buffer (*)(T, Buffer);
//     dispatch_type fnc   = nullptr;
//     (void)((Members::index == msg.header.opcode ? fnc = &Members::dispatch, true : false) || ...);
//     return fnc(std::forward<T>(obj), msg);
//   }
// };

template <typename T, typename... Members>
constexpr auto do_dispatch(T&& obj, std::size_t index, std::span<char const> args) {
  constexpr static int strategy = sizeof...(Members) <= 4    ? 1
                                  : sizeof...(Members) <= 16 ? 2
                                  : sizeof...(Members) <= 64 ? 3
                                                             // : sizeof...(Members) <= 256 ? 4
                                                             : 4;
  return DispatchStrategy<strategy>::template dispatch<T, Members...>(std::forward<T>(obj), index, args);
}
}  // namespace impl

template <typename T, typename U = void>
using Proxy = [:impl::make_proxy_t<std::remove_cvref_t<T>, U>():];

template <typename T, typename U>
auto make_proxy(U* client) {
  return Proxy<T, U>{client};
}

namespace impl {
template <typename T, typename Protocol>
consteval auto get_dispatcher() {
  using proto = std::remove_cvref_t<Protocol>;

  std::vector args = {^^T};
  for (auto member : nonstatic_data_members_of(^^Proxy<std::remove_cvref_t<T>, proto>)) {
    args.push_back(type_of(member));
  }
  using fnc_type = typename proto::message_type (*)(T&&, std::size_t, std::span<char const>);
  return extract<fnc_type>(substitute(^^do_dispatch, args));
}
}  // namespace impl

}  // namespace erl::rpc
