#pragma once
#include <cassert>
#include <cstddef>
#include <type_traits>
#include <vector>
#include <concepts>
#include <ranges>
#include <experimental/meta>

#include <erl/reflect>
#include <erl/_impl/util/meta.hpp>
#include <erl/_impl/util/stamp.hpp>
#include <erl/_impl/net/message/buffer.hpp>
#include <erl/_impl/net/message/reader.hpp>
#include "dispatch.hpp"

namespace erl::rpc {
namespace annotations {
struct Handler {
  std::meta::info fnc;
  friend consteval bool operator==(Handler const& self, std::optional<Handler> const& other) {
    if (!other.has_value()) {
      return false;
    }
    return self.fnc == other->fnc;
  }
};

consteval Handler handler(std::meta::info fnc) {
  return {fnc};
}

struct CallbackTag {
} constexpr inline callback{};
}  // namespace annotations

template <typename Service, int Idx, typename Super, typename R>
struct FunctionProxy {
  template <typename... Ts>
  decltype(auto) operator()(Ts&&... args) const {
    // get a pointer to Proxy superobject from this subobject
    constexpr static auto current_member = meta::get_nth_member(^^Super, Idx + 1);
    Super* that = reinterpret_cast<Super*>(std::uintptr_t(this) - offset_of(current_member).bytes);

    // first (unnamed) member of Proxy is a pointer to the actual handler
    auto* handler = that->[:meta::get_nth_member(^^Super, 0):];
    return handler->template call<Service, R>(Idx, std::forward<Ts>(args)...);
  }
};

template <std::meta::info Meta, int Idx, typename Protocol>
struct FunctionDispatcher {
  template <typename Obj>
    requires(parent_of(Meta) == remove_cvref(^^Obj))
  static constexpr decltype(auto) eval(Obj&& obj, std::span<char const> data) {
    auto args = message::MessageView{data};
    return [:meta::expand(parameters_of(Meta)):] >> [&]<auto... Params> {
      return (std::forward<Obj>(obj).[:Meta:])(deserialize<[:type_of(Params):]>(args)...);
    };
  }

  template <typename Obj>
  static constexpr Protocol::message_type dispatch(Obj&& obj, std::span<char const> data) {
    if constexpr (return_type_of(Meta) == ^^void) {
      eval(std::forward<Obj>(obj), data);
      return Protocol::make_response(Idx);
    } else {
      return Protocol::make_response(Idx, eval(std::forward<Obj>(obj), data));
    }
  }
};

template <typename Service, int Idx, typename Super, typename R, std::meta::info H>
struct CustomProxy {
  template <typename... Ts>
  decltype(auto) operator()(Ts&&... args) const {
    // get a pointer to Proxy superobject from this subobject
    constexpr static auto current_member = meta::get_nth_member(^^Super, Idx + 1);
    Super* that = reinterpret_cast<Super*>(std::uintptr_t(this) - offset_of(current_member).bytes);

    // first (unnamed) member of Proxy is a pointer to the actual handler
    auto* handler = that->[:meta::get_nth_member(^^Super, 0):];

    // TODO find better way to substitute function template in
    // this assumes the only template arguments are a trailing pack
    constexpr static auto non_template_args = parameters_of(substitute(H, {})).size();
    auto message = [:substitute(H, std::vector{^^Ts...} | std::views::drop(non_template_args)):](std::forward<Ts>(args)...);
    return handler->template call<Service, R>(Idx, message);
  }
};

template <auto H, int Idx, typename Protocol>
struct CustomDispatcher {
  template <typename Obj>
  static constexpr decltype(auto) eval(Obj&& obj, std::span<char const> data) {
    return (std::forward<Obj>(obj).[:H:])(data);
  }

  template <typename Obj>
  static constexpr Protocol::message_type dispatch(Obj&& obj, std::span<char const> data) {
    if constexpr (return_type_of(H) == ^^void) {
      eval(std::forward<Obj>(obj), data);
      return Protocol::make_response(Idx);
    } else {
      return Protocol::make_response(Idx, eval(std::forward<Obj>(obj), data));
    }
  }
};

template <std::meta::info Meta, int Idx, typename Super>
struct FunctionTemplateProxy {
  template <typename R, typename S, typename... Args>
  static R call(S& service, std::span<char const> data) {
    constexpr auto base = substitute(Meta, {});
    constexpr auto skip = parameters_of(base).size();

    return [:meta::expand(std::ranges::iota_view{skip, sizeof...(Args)}):] >> [&]<auto... Is> {
      static_assert(can_substitute(Meta, {^^Args...[Is]...}), "Invalid template callback");

      constexpr auto target = substitute(Meta, {^^Args...[Is]...});
      auto reader           = erl::message::MessageView{data};
      return service.[:target:](deserialize<Args>(reader)...);
    };
  }

  template <typename... Args>
  decltype(auto) operator()(Args&&... args) const {
    // get a pointer to Proxy superobject from this subobject
    constexpr static auto current_member = meta::get_nth_member(^^Super, Idx + 1);
    Super* that = reinterpret_cast<Super*>(std::uintptr_t(this) - offset_of(current_member).bytes);

    // first (unnamed) member of Proxy is a pointer to the actual handler
    auto* handler = that->[:meta::get_nth_member(^^Super, 0):];

    constexpr auto base = substitute(Meta, {});
    constexpr auto skip = parameters_of(base).size();

    using R = [:[:meta::expand(std::ranges::iota_view{skip, sizeof...(Args)}):] >> []<auto... Is> {
      static_assert(can_substitute(Meta, {^^Args...[Is]...}), "Invalid template callback");
      constexpr auto result = invoke_result(type_of(substitute(Meta, {^^Args...[Is]...})), {^^Args...});
      // constexpr auto base_result = invoke_result(type_of(base), {});
      // static_assert(result == base_result, "Inconsistent return type");

      return result;
    }:];

    // accept Service as const& for const qualified member functions
    using S = [:is_const(Meta) ? add_const(parent_of(Meta)) : parent_of(Meta):];

    return handler->template call<std::remove_const_t<S>, R>(
        Idx, &FunctionTemplateProxy::call<R, S, std::remove_cvref_t<Args>...>, std::forward<Args>(args)...);
  }
};

template <std::meta::info Meta, int Idx, typename Protocol>
struct FunctionTemplateDispatcher {
  using return_type  = [:[:meta::expand(parameters_of(substitute(Meta, {}))):] >> []<auto... Params> {
    return invoke_result(type_of(substitute(Meta, {})), {type_of(Params)...});
  }:];
  using service_type = [:is_const(Meta) ? add_const(parent_of(Meta)) : parent_of(Meta):];
  using fnc_type     = return_type (*)(service_type&, std::span<char const>);

  template <typename Obj>
    requires(parent_of(Meta) == remove_cvref(^^Obj))
  static constexpr decltype(auto) eval(Obj&& obj, Deserializer auto& args) {
    fnc_type wrapper = deserialize<fnc_type>(args);

    // TODO remove dependency on .buffer being a span
    // do not forward obj - wrapper expects a lvalue reference
    return wrapper(obj, args.buffer.subspan(args.cursor));
  }

  template <typename Obj>
  static constexpr Protocol::message_type dispatch(Obj&& obj, std::span<char const> data) {
    constexpr auto base = substitute(Meta, {});
    auto args           = message::MessageView{data};

    if constexpr (std::same_as<return_type, void>) {
      eval(std::forward<Obj>(obj), args);
      return Protocol::make_response(Idx);
    } else {
      return Protocol::make_response(Idx, eval(std::forward<Obj>(obj), args));
    }
  }
};

template <typename... Ps>
struct Policy {
  template <typename Service, typename Client>
  consteval auto make_proxy(this auto&& self, std::meta::info proxy) {
    std::vector args = {data_member_spec(^^Client*)};
    int index        = 0;

    for (auto member_fnc : meta::named_members_of(^^Service)) {
      std::meta::info member;

      if (((Ps::is_remote(member_fnc) &&
            ((member = Ps::template make_proxy_member<Service>(proxy, index++, member_fnc)) != std::meta::info{})) ||
           ...)) {
        args.push_back(member);
      }
    }
    return args;
  }

  template <typename Service, typename Protocol>
  consteval auto make_dispatcher(this auto&& self) {
    std::vector<std::meta::info> args{};
    int index = 0;

    for (auto member_fnc : meta::named_members_of(^^Service)) {
      std::meta::info member;

      if (((Ps::is_remote(member_fnc) && ((member = Ps::template make_dispatch_member<Service, Protocol>(
                                               index++, member_fnc)) != std::meta::info{})) ||
           ...)) {
        args.push_back(member);
      }
    }
    return args;
  }
};

namespace policies {
struct DefaultPolicy : Policy<DefaultPolicy> {
  consteval static bool is_remote(std::meta::info member) { return is_public(member) && is_function(member); }

  template <typename Service>
  consteval static std::meta::info make_proxy_member(std::meta::info proxy, int index, std::meta::info fnc) {
    auto idx  = std::meta::reflect_value(index);
    auto type = substitute(^^FunctionProxy, {^^Service, idx, proxy, return_type_of(fnc)});
    return data_member_spec(type, {.name = identifier_of(fnc), .no_unique_address=true});
  }

  template <typename Service, typename Protocol>
  consteval static std::meta::info make_dispatch_member(int index, std::meta::info fnc) {
    auto idx = std::meta::reflect_value(index);
    return substitute(^^FunctionDispatcher, {reflect_value(fnc), idx, ^^Protocol});
  }
};

struct InProcess : Policy<InProcess> {
  consteval static bool is_remote(std::meta::info member) {
    if (!is_public(member)) {
      return false;
    }

    // only include function templates that require zero or more arguments
    // we can only use function templates with a trailing parameter pack
    return is_function_template(member) && can_substitute(member, {});
  }

  template <typename Service>
  consteval static std::meta::info make_proxy_member(std::meta::info proxy, int index, std::meta::info fnc) {
    auto idx             = std::meta::reflect_value(index);
    std::meta::info type = substitute(^^FunctionTemplateProxy, {reflect_value(fnc), idx, proxy});
    return data_member_spec(type, {.name = identifier_of(fnc), .no_unique_address=true});
  }

  template <typename Service, typename Protocol>
  consteval static std::meta::info make_dispatch_member(int index, std::meta::info fnc) {
    auto idx = std::meta::reflect_value(index);
    return substitute(^^FunctionTemplateDispatcher, {reflect_value(fnc), idx, ^^Protocol});
  }
};

struct Annotated : Policy<Annotated> {
  consteval static bool is_remote(std::meta::info member) {
    if (!is_public(member)) {
      return false;
    }

    if (is_function(member)) {
      return meta::has_annotation<annotations::CallbackTag>(member) ||
             meta::has_annotation<annotations::Handler>(member);
    }

    if (is_function_template(member) && can_substitute(member, {})) {
      // TODO must be static
      // TODO must return message type
      return meta::has_annotation<annotations::Handler>(substitute(member, {}));
    }
    return false;
  }

  consteval static auto return_of(std::meta::info member) {
    if (auto handler = annotation_of_type<annotations::Handler>(member); handler) {
      return return_type_of(handler->fnc);
    }
    return return_type_of(member);
  }

  template <typename Service>
  consteval static std::meta::info make_proxy_member(std::meta::info proxy, int index, std::meta::info fnc) {
    // return type of public handler will always be the message type
    // TODO grab actual return type from handler

    auto idx = std::meta::reflect_value(index);
    std::meta::info type;
    if (is_function(fnc)) {
      if (meta::has_annotation<annotations::Handler>(fnc)) {
        type = substitute(^^CustomProxy, {^^Service, idx, proxy, return_of(fnc), reflect_value(fnc)});
      } else {
        type = substitute(^^FunctionProxy, {^^Service, idx, proxy, return_of(fnc)});
      }
    } else {
      auto base = substitute(fnc, {});
      if (!meta::has_annotation<annotations::Handler>(base)) {
        return {};
      }

      type = substitute(^^CustomProxy, {^^Service, idx, proxy, return_of(base), reflect_value(fnc)});
    }

    return data_member_spec(type, {.name = identifier_of(fnc), .no_unique_address=true});
  }

  template <typename Service, typename Protocol>
  consteval static std::meta::info make_dispatch_member(int index, std::meta::info fnc) {
    auto idx = std::meta::reflect_value(index);
    if (is_function(fnc)) {
      if (auto handler = annotation_of_type<annotations::Handler>(fnc); handler) {
        return substitute(^^CustomDispatcher, {reflect_value(handler->fnc), idx, ^^Protocol});
      }
      return substitute(^^FunctionDispatcher, {reflect_value(fnc), idx, ^^Protocol});
    } else {
      auto base = substitute(fnc, {});
      if (auto handler = annotation_of_type<annotations::Handler>(base); handler) {
        return substitute(^^CustomDispatcher, {reflect_value(handler->fnc), idx, ^^Protocol});
      }
      std::unreachable();
    }
  }
};
}  // namespace policies

namespace _impl {
template <typename Service, typename Client>
consteval auto make_proxy_t() {
  struct Proxy;
  consteval {
    auto policy = typename Service::policy{};
    define_aggregate(^^Proxy, policy.template make_proxy<Service, Client>(^^Proxy));
  }
  static_assert(is_type(^^Proxy), "Could not inject RPC proxy class");
  return ^^Proxy;
}

template <typename... Members>
struct Dispatcher {
  template <typename T>
  constexpr static auto operator()(T&& obj, std::size_t index, std::span<char const> args) {
    static_assert(sizeof...(Members) <= 256, "Too many callbacks");
    constexpr static int strategy_idx = sizeof...(Members) <= 4    ? 1
                                        : sizeof...(Members) <= 16 ? 2
                                        : sizeof...(Members) <= 64 ? 3
                                                                   : 4;
    using strategy                    = _dispatch_impl::DispatchStrategy<strategy_idx>;
    return strategy::template dispatch<T, Members...>(std::forward<T>(obj), index, args);
  }
};

template <typename Service, typename Protocol>
consteval auto make_dispatcher_t() {
  auto policy = typename Service::policy{};
  return substitute(^^Dispatcher, policy.template make_dispatcher<Service, Protocol>());
}
}  // namespace _impl

template <typename T, typename U>
using Proxy = [:_impl::make_proxy_t<std::remove_cvref_t<T>, U>():];

template <typename T, typename U>
auto make_proxy(U* client) {
  return Proxy<T, U>{client};
}

template <typename T, typename Protocol>
using Dispatcher = [:_impl::make_dispatcher_t<std::remove_cvref_t<T>, Protocol>():];

}  // namespace erl::rpc
