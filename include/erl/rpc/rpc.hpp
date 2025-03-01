#pragma once
#include <cstddef>
#include <type_traits>
#include <vector>
#include <concepts>
#include <ranges>
#include <experimental/meta>

#include <erl/reflect.hpp>
#include <erl/util/meta.hpp>
#include <erl/util/stamp.hpp>
#include <erl/net/message/buffer.hpp>
#include <erl/net/message/reader.hpp>
#include "dispatch.hpp"

namespace erl::rpc {
inline namespace annotations {
struct Handler {
  std::meta::info fnc;
  friend bool operator==(Handler const& self, std::optional<Handler> const& other) {
    if (!other.has_value()) {
      return false;
    }
    return self.fnc == other->fnc;
  }
};

consteval Handler handler(std::meta::info fnc) {
  return {fnc};
}

struct SkipTag {};
constexpr inline SkipTag skip{};
}  // namespace annotations

template <int Idx, typename Super, typename R>
struct FunctionProxy {
  template <typename... Ts>
  decltype(auto) operator()(Ts&&... args) const {
    // get a pointer to Proxy superobject from this subobject
    constexpr static auto current_member = meta::get_nth_member(^^Super, Idx + 1);
    Super* that = reinterpret_cast<Super*>(std::uintptr_t(this) - offset_of(current_member).bytes);

    // first (unnamed) member of Proxy is a pointer to the actual handler
    auto* handler = that->[:meta::get_nth_member(^^Super, 0):];
    return handler->template call<R>(Idx, std::forward<Ts>(args)...);
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

template <auto H, int Idx, typename Protocol>
struct CustomDispatcher {
  template <typename Obj>
  static constexpr decltype(auto) eval(Obj&& obj, std::span<char const> data) {
    // TODO figure out why it needs to be offset by 1
    return (std::forward<Obj>(obj).[:H:])(data.subspan(1));
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

    return handler->template call<R>(Idx, &FunctionTemplateProxy::call<R, S, std::remove_cvref_t<Args>...>,
                                     std::forward<Args>(args)...);
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

struct Policy {
  template <typename Policy>
  consteval auto get_remote_functions(this Policy&& self, std::meta::info type) {
    return meta::named_members_of(type) | std::views::filter(std::meta::is_public) | std::views::filter(self) |
           std::ranges::to<std::vector>();
  }

  template <typename Self>
  consteval bool operator()(this Self&& self, std::meta::info member) {
    return std::forward<Self>(self).is_remote(member);
  }

  template <typename Service, typename Client>
  consteval auto make_proxy(this auto&& self, std::meta::info proxy) {
    std::vector args = {data_member_spec(^^Client*)};
    int index        = 0;

    for (auto member_fnc : self.get_remote_functions(^^Service)) {
      auto member = self.template make_proxy_member<Service>(proxy, index++, member_fnc);
      args.push_back(member);
    }
    return args;
  }

  template <typename Service, typename Protocol>
  consteval auto make_dispatcher(this auto&& self) {
    std::vector<std::meta::info> args{};
    int index = 0;

    for (auto member_fnc : self.get_remote_functions(^^Service)) {
      auto member = self.template make_dispatch_member<Service, Protocol>(index++, member_fnc);
      args.push_back(member);
    }
    return args;
  }
};

inline namespace policies {
struct DefaultPolicy : Policy {
  consteval bool is_remote(std::meta::info member) const { return is_function(member); }

  template <typename Service>
  consteval auto make_proxy_member(std::meta::info proxy, int index, std::meta::info fnc) const {
    auto idx  = std::meta::reflect_value(index);
    auto type = substitute(^^FunctionProxy, {idx, proxy, return_type_of(fnc)});
    return data_member_spec(type, {.name = identifier_of(fnc)});
  }

  template <typename Service, typename Protocol>
  consteval auto make_dispatch_member(int index, std::meta::info fnc) const {
    auto idx = std::meta::reflect_value(index);
    return substitute(^^FunctionDispatcher, {reflect_value(fnc), idx, ^^Protocol});
  }
};

struct InProcess : Policy {
  consteval bool is_remote(std::meta::info member) const {
    if (is_function(member)) {
      return true;
    }
    // only include function templates that require zero or more arguments
    // we can only use function templates with a trailing parameter pack
    return is_function_template(member) && can_substitute(member, {});
  }

  template <typename Service>
  consteval auto make_proxy_member(std::meta::info proxy, int index, std::meta::info fnc) const {
    auto idx = std::meta::reflect_value(index);
    std::meta::info type;
    if (is_function(fnc)) {
      type = substitute(^^FunctionProxy, {idx, proxy, return_type_of(fnc)});
    } else {
      type = substitute(^^FunctionTemplateProxy, {reflect_value(fnc), idx, proxy});
    }
    return data_member_spec(type, {.name = identifier_of(fnc)});
  }

  template <typename Service, typename Protocol>
  consteval auto make_dispatch_member(int index, std::meta::info fnc) const {
    auto idx = std::meta::reflect_value(index);
    if (is_function(fnc)) {
      return substitute(^^FunctionDispatcher, {reflect_value(fnc), idx, ^^Protocol});
    } else {
      return substitute(^^FunctionTemplateDispatcher, {reflect_value(fnc), idx, ^^Protocol});
    }
  }
};

struct Annotated : Policy {
  consteval bool is_remote(std::meta::info member) const {
    if (is_function(member)) {
      return !meta::has_annotation(member, annotations::skip);
    }

    if (is_function_template(member) && can_substitute(member, {})) {
      return meta::has_annotation<annotations::Handler>(substitute(member, {}));
    }
    return false;
  }

  consteval auto return_of(std::meta::info member) const {
    if (meta::has_annotation<annotations::Handler>(member)) {
      auto handler = *annotation_of_type<annotations::Handler>(member);
      return return_type_of(handler.fnc);
    }
    return return_type_of(member);
  }

  template <typename Service>
  consteval auto make_proxy_member(std::meta::info proxy, int index, std::meta::info fnc) const {
    auto idx = std::meta::reflect_value(index);
    std::meta::info type;
    if (is_function(fnc)) {
      type = substitute(^^FunctionProxy, {idx, proxy, return_of(fnc)});
    } else {
      auto base = substitute(fnc, {});
      type = substitute(^^FunctionProxy, {idx, proxy, return_of(base)});
    }

    return data_member_spec(type, {.name = identifier_of(fnc)});
  }

  template <typename Service, typename Protocol>
  consteval auto make_dispatch_member(int index, std::meta::info fnc) const {
    auto idx        = std::meta::reflect_value(index);
    if (is_function(fnc)) {
      if (meta::has_annotation(fnc, ^^annotations::handler)) {
        auto handler = *annotation_of_type<annotations::Handler>(fnc);
        return substitute(^^CustomDispatcher, {reflect_value(handler.fnc), idx, ^^Protocol});
      }
      return substitute(^^FunctionDispatcher, {reflect_value(fnc), idx, ^^Protocol});
    } else {
      auto base = substitute(fnc, {});
      auto handler = *annotation_of_type<annotations::Handler>(base);
      return substitute(^^CustomDispatcher, {reflect_value(handler.fnc), idx, ^^Protocol});
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
