#pragma once
#include <experimental/meta>
#include <ranges>
#include <type_traits>
#include <vector>

#include "reflect.hpp"
#include "util/meta.hpp"
#include "util/stamp.hpp"

namespace rpc {
namespace impl {
template <std::meta::info Meta, int, typename, typename = [:type_of(Meta):]>
struct CallProxy;

template <std::meta::info Meta, int Idx, typename Super, typename R,
          typename... Args>
struct CallProxy<Meta, Idx, Super, R(Args...)> {
  static constexpr auto index = Idx;

  template <typename Obj> static constexpr R eval(Obj &&obj, Message msg) {
    auto parser = MessageParser{msg.payload()};
    return (std::forward<Obj>(obj).[:Meta:])(parser.template get_field<Args>()...);
  }

  decltype(auto) operator()(Args... args) const
    requires(!std::same_as<Super, void>) // only available if proxy is bound
  {
    auto builder = Message{{.kind = static_cast<Message::Flags>(
                                Message::Flags::FNC | Message::Flags::REQ),
                            .opcode = Idx}};
    (builder.add_field(args), ...);
    auto request = builder.finalize();

    // move along, nothing to see here

    // get a pointer to Proxy superobject from this subobject
    constexpr static auto member = meta::nth_nsdm<Super>(Idx + 1);
    Super *that = reinterpret_cast<Super *>(std::uintptr_t(this) -
                                            offset_of(member).bytes);

    // first (unnamed) member of Proxy is a pointer to the actual handler
    auto *handler = that->[:meta::nth_nsdm<Super>(0):];
    return handler->template call<R>(request);
  }

  template <typename Obj>
  static constexpr Message dispatch(Obj &&obj, Message msg) {
    auto ret = Message{{.kind = static_cast<Message::Flags>(
                            Message::Flags::FNC | Message::Flags::RET),
                        .opcode = msg.header.opcode}};
    if constexpr (std::is_void_v<R>) {
      eval(std::forward<Obj>(obj), msg);
    } else {
      auto retval = eval(std::forward<Obj>(obj), msg);
      ret.add_field(retval);
    }
    return ret.finalize();
  }
};

template <std::meta::info Meta, int Idx, typename Super, typename R,
          typename... Args>
struct CallProxy<Meta, Idx, Super, R(Args...) const>
    : CallProxy<Meta, Idx, Super, R(Args...)> {};

template <typename T, typename U> consteval auto make_proxy_t() {
  struct Proxy;

  std::vector args = {data_member_spec(^U *)};
  int index = 0;

  for (auto member_fnc : meta::member_functions_of(^T)) {
    auto type =
        substitute(^CallProxy, {
                                   reflect_value(member_fnc),
                                   std::meta::reflect_value(index++), ^Proxy});
    args.push_back(data_member_spec(type, {.name = identifier_of(member_fnc)}));
  }

  return define_aggregate(^Proxy, args);
}

#define $generate_case(Idx)                                                    \
  case (Idx):                                                                  \
    if constexpr ((Idx) < sizeof...(Members) - 1) {                            \
      using call_type = Members...[Idx + 1];                                   \
      return call_type::dispatch(std::forward<T>(obj), msg);                   \
    }                                                                          \
    std::unreachable();

#define $generate_strategy(Idx, Stamper)                                       \
  template <> struct DispatchStrategy<Idx> {                                   \
    template <typename T, typename... Members>                                 \
    static constexpr Message dispatch(T &&obj, Message msg) {                  \
      switch (msg.header.opcode) { Stamper(0, $generate_case); }               \
      std::unreachable();                                                      \
    }                                                                          \
  }

template <int> struct DispatchStrategy;
$generate_strategy(1, $stamp(4));   // 4^1 potential states
$generate_strategy(2, $stamp(16));  // 4^2 potential states
$generate_strategy(3, $stamp(64));  // 4^3 potential states
$generate_strategy(4, $stamp(256)); // 4^4 potential states

#undef $generate_strategy
#undef $generate_case

template <> struct DispatchStrategy<0> {
  // fallback
  template <typename T, typename... Members>
  static constexpr Message dispatch(T &&obj, Message msg) {
    using dispatch_type = Message (*)(T, Message);
    dispatch_type fnc = nullptr;
    (void)((Members::index == msg.header.opcode ? fnc = &Members::dispatch,
            true                                : false) ||
           ...);
    return fnc(std::forward<T>(obj), msg);
  }
};

template <typename T, typename... Members>
constexpr Message do_dispatch(T &&obj, Message msg) {
  constexpr static int strategy = sizeof...(Members) <= 4     ? 1
                                  : sizeof...(Members) <= 16  ? 2
                                  : sizeof...(Members) <= 64  ? 3
                                  : sizeof...(Members) <= 256 ? 4
                                                              : 0;
  return DispatchStrategy<strategy>::template dispatch<T, Members...>(
      std::forward<T>(obj), msg);
}
} // namespace impl

template <typename T, typename U = void>
using Proxy = [:impl::make_proxy_t<std::remove_cvref_t<T>, U>():];

template <typename T, typename U> auto make_proxy(U &&client) {
  return Proxy<T, std::remove_cvref_t<U>>{&client};
}

namespace impl {
template <typename T> consteval auto get_dispatcher() {
  std::vector args = {^T};
  for (auto member : nonstatic_data_members_of(^Proxy<T>)) {
    args.push_back(type_of(member));
  }
  return extract<Message (*)(T &&, Message)>(substitute(^do_dispatch, args));
}
} // namespace impl

struct Service {
  template <typename T> auto dispatch(this T &&self, Message msg) {
    constexpr static auto dispatcher = impl::get_dispatcher<T>();
    return dispatcher(std::forward<T>(self), msg);
  }
};
} // namespace rpc