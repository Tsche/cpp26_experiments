#pragma once
#include <optional>
#include <string_view>
#include <experimental/meta>

#include <rsl/string_view>

#include <erl/_impl/expect.hpp>

namespace erl::annotations {
constexpr inline struct Option {} option {};
constexpr inline struct Descend {} descend {};

struct StringAnnotation {
  rsl::string_view data;

  consteval explicit StringAnnotation(std::string_view text) : data(std::define_static_string(text)) {}
  constexpr operator std::string_view() const {
    return {data.data(), data.size()};
  }
  
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