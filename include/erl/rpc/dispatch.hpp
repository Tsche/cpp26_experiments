s#pragma once
#include <stdexcept>

namespace erl::rpc::_dispatch_impl {
#define $generate_case(Idx)                                   \
  case (Idx):                                                 \
    if constexpr ((Idx) < sizeof...(Members)) {           \
      using call_type = Members...[Idx];                      \
      return call_type::dispatch(std::forward<T>(obj), args); \
    }                                                         \
    std::unreachable();

#define $generate_strategy(Idx, Stamper)                                                     \
  template <>                                                                                \
  struct DispatchStrategy<(Idx)> {                                                           \
    template <typename T, typename... Members>                                               \
    static constexpr auto dispatch(T&& obj, std::size_t index, std::span<char const> args) { \
      switch (index) {                                                                       \
        Stamper(0, $generate_case);                                                          \
        default: throw std::out_of_range("Invalid opcode");                                  \
      }                                                                                      \
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

}  // namespace erl::rpc::_dispatch_impl
