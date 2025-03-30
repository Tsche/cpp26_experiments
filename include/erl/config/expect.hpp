#pragma once
#include <experimental/meta>

#include <concepts>
#include <string>
#include <format>

#include <erl/tuple.hpp>
#include <erl/util/concepts.hpp>
#include <erl/util/meta.hpp>
#include <erl/util/operators.hpp>


namespace erl::_expect_impl {

template <typename T>
consteval auto consteval_wrap(T const& value){
    if constexpr (std::convertible_to<T, std::string_view>){
        return std::meta::define_static_string(value);
    } else {
        return value;
    }
}
template <typename T>
using wrapped_t = decltype(consteval_wrap(std::declval<T>()));

#define $make_op(class_name, op) \
    template <typename T>\
    constexpr friend auto operator op(class_name rhs_, T lhs_) {\
        if consteval {\
            return BinaryExpr<util::to_operator(#op), class_name, wrapped_t<T>>{rhs_, consteval_wrap(lhs_)};\
        } else {\
            return BinaryExpr<util::to_operator(#op), class_name, T>{rhs_, lhs_};\
        }\
    }\
    template <typename T>\
    constexpr friend auto operator op(T rhs_, class_name lhs_) requires (!util::is_specialization<T, ^^erl::_expect_impl:: class_name>) {\
        if consteval {\
            return BinaryExpr<util::to_operator(#op), wrapped_t<T>, class_name>{consteval_wrap(rhs_), lhs_};\
        } else {\
            return BinaryExpr<util::to_operator(#op), T, class_name>{rhs_, lhs_};\
        }\
    }\
\




template <std::meta::operators OP, typename Rhs, typename Lhs>
struct BinaryExpr {
    Rhs rhs;
    Lhs lhs;

    // comparisons
    $make_op(BinaryExpr, <);
    $make_op(BinaryExpr, <=);
    $make_op(BinaryExpr, >);
    $make_op(BinaryExpr, >=);
    $make_op(BinaryExpr, ==);
    $make_op(BinaryExpr, !=);

    // logical
    $make_op(BinaryExpr, &&);
    $make_op(BinaryExpr, ||);

    constexpr decltype(auto) eval(auto const& args) {
        static constexpr bool rhs_evaluatable = requires { {rhs.eval(args)} -> util::is_non_void; };
        static constexpr bool lhs_evaluatable = requires { {lhs.eval(args)} -> util::is_non_void; };
        if constexpr (rhs_evaluatable && lhs_evaluatable){
            return eval_operator(rhs.eval(args), lhs.eval(args));
        } else if constexpr (rhs_evaluatable) {
            return eval_operator(rhs.eval(args), lhs);
        } else if constexpr (lhs_evaluatable) {
            return eval_operator(rhs, lhs.eval(args));
        }
    }

    constexpr std::string print(auto const& args) {
        static constexpr bool rhs_printable = requires { {rhs.print(args)} -> std::same_as<std::string>; };
        static constexpr bool lhs_printable = requires { {lhs.print(args)} -> std::same_as<std::string>; };

        if constexpr (rhs_printable && lhs_printable){
            return std::format("({} {} {})", rhs.print(args), util::to_string(OP), lhs.print(args));
        } else if constexpr (rhs_printable) {
            return std::format("({} {} {})", rhs.print(args), util::to_string(OP), lhs);
        } else if constexpr (lhs_printable) {
            return std::format("({} {} {})", rhs, util::to_string(OP), lhs.print(args));
        }
    }

    static constexpr decltype(auto) eval_operator(auto lhs, auto rhs) {
        using enum std::meta::operators;
        /* */if constexpr (OP == op_plus                   ) { return lhs + rhs; }
        else if constexpr (OP == op_minus                  ) { return lhs - rhs; }
        else if constexpr (OP == op_star                   ) { return lhs * rhs; }
        else if constexpr (OP == op_slash                  ) { return lhs / rhs; }
        else if constexpr (OP == op_percent                ) { return lhs % rhs; }
        else if constexpr (OP == op_caret                  ) { return lhs ^ rhs; }
        else if constexpr (OP == op_ampersand              ) { return lhs & rhs; }
        else if constexpr (OP == op_pipe                   ) { return lhs | rhs; }
        else if constexpr (OP == op_equals                 ) { return lhs = rhs; }
        else if constexpr (OP == op_plus_equals            ) { return lhs += rhs; }
        else if constexpr (OP == op_minus_equals           ) { return lhs -= rhs; }
        else if constexpr (OP == op_star_equals            ) { return lhs *= rhs; }
        else if constexpr (OP == op_slash_equals           ) { return lhs /= rhs; }
        else if constexpr (OP == op_percent_equals         ) { return lhs %= rhs; }
        else if constexpr (OP == op_caret_equals           ) { return lhs ^= rhs; }
        else if constexpr (OP == op_ampersand_equals       ) { return lhs &= rhs; }
        else if constexpr (OP == op_pipe_equals            ) { return lhs |= rhs; }
        else if constexpr (OP == op_equals_equals          ) { return lhs == rhs; }
        else if constexpr (OP == op_exclamation_equals     ) { return lhs != rhs; }
        else if constexpr (OP == op_less                   ) { return lhs < rhs; }
        else if constexpr (OP == op_greater                ) { return lhs > rhs; }
        else if constexpr (OP == op_less_equals            ) { return lhs <= rhs; }
        else if constexpr (OP == op_greater_equals         ) { return lhs >= rhs; }
        else if constexpr (OP == op_spaceship              ) { return lhs <=> rhs; }
        else if constexpr (OP == op_ampersand_ampersand    ) { return lhs && rhs; }
        else if constexpr (OP == op_pipe_pipe              ) { return lhs || rhs; }
        else if constexpr (OP == op_less_less              ) { return lhs << rhs; }
        else if constexpr (OP == op_greater_greater        ) { return lhs >> rhs; }
        else if constexpr (OP == op_less_less_equals       ) { return lhs <<= rhs; }
        else if constexpr (OP == op_greater_greater_equals ) { return lhs >>= rhs; }
        else if constexpr (OP == op_comma                  ) { return lhs , rhs; }
    }
};


template <std::size_t Idx>
struct Placeholder {
    // comparisons
    $make_op(Placeholder, <);
    $make_op(Placeholder, <=);
    $make_op(Placeholder, >);
    $make_op(Placeholder, >=);
    $make_op(Placeholder, ==);
    $make_op(Placeholder, !=);

    // logical
    $make_op(Placeholder, &&);
    $make_op(Placeholder, ||);

    constexpr decltype(auto) eval(auto const& args){
        static constexpr auto placeholder_count = std::tuple_size_v<std::remove_cvref_t<decltype(args)>>;
        static_assert(Idx < placeholder_count, 
            std::string("Placeholder $") + util::utos(Idx) + " has no associated value");
        return get<Idx>(args);
    }

    constexpr std::string print(auto const& args) {
        return std::format("{}", std::get<Idx>(args));
    }
};

template <typename F, typename... Args>
struct Lazy {
    F callable;
    std::tuple<Args...> arguments;

    // comparisons
    $make_op(Lazy, <);
    $make_op(Lazy, <=);
    $make_op(Lazy, >);
    $make_op(Lazy, >=);
    $make_op(Lazy, ==);
    $make_op(Lazy, !=);

    // logical
    $make_op(Lazy, &&);
    $make_op(Lazy, ||);

    template <std::size_t Idx>
    constexpr decltype(auto) expand_args(auto const& args){
        using wrapped_t = Args...[Idx];
        if constexpr (requires (wrapped_t obj) { obj.eval(args); }){
            return get<Idx>(arguments).eval(args);
        } else {
            return get<Idx>(arguments);
        }
    }

    constexpr decltype(auto) eval(auto const& args){
        return [:meta::sequence(sizeof...(Args)):]
            >> [&]<auto... Idx>{
                return callable(expand_args<Idx>(args)...);
            };
    }

    constexpr std::string print(auto const& args) {
        return "<lazy call>";
    }
};


#define LAZY_ARGS(...) erl::Tuple(__VA_ARGS__) }
#define $(fnc) Lazy{[]<typename... Ts>(Ts&&... args){ return fnc(std::forward<Ts>(args)...); }, LAZY_ARGS

struct LazyProxy {
    template <typename Fn>
    constexpr auto operator()(Fn fn) const {
        return [fn](auto&&... args) {
            return Lazy{fn, erl::Tuple(args...)};
        };
    }
};
}