#pragma once
#include <experimental/meta>
#include <functional>

namespace meta {
    consteval auto member_functions_of(std::meta::info reflection){
    return members_of(reflection) 
        | std::views::filter(std::meta::is_public) 
        | std::views::filter(std::meta::is_function) 
        | std::views::filter(std::not_fn(std::meta::is_special_member_function))
        | std::views::filter(std::not_fn(std::meta::is_conversion_function))
        | std::views::filter(std::not_fn(std::meta::is_operator_function))
        | std::views::filter(std::not_fn(std::meta::is_static_member))
        | std::ranges::to<std::vector>();
}
}