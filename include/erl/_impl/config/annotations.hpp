#pragma once
#include <optional>
#include <string_view>
#include <experimental/meta>

#include <erl/_impl/expect.hpp>

namespace erl::annotations {
constexpr inline struct Option {} option {};

struct StringAnnotation {
  char const* data;
  consteval explicit StringAnnotation(std::string_view text) : data(std::meta::define_static_string(text)){}
  friend consteval bool operator==(StringAnnotation const& self, std::optional<StringAnnotation> const& other) {
    if (!other.has_value()) {
      return false;
    }
    return self.data == other->data;
  }
};

struct Shorthand : StringAnnotation {
    using StringAnnotation::StringAnnotation;
};

struct Description : StringAnnotation {
    using StringAnnotation::StringAnnotation;
};

using erl::_expect_impl::Expect;

}  // namespace erl::annotations