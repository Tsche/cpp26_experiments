#pragma once
#include <optional>
#include <string_view>
#include <experimental/meta>


namespace erl::annotations {
struct Option {};

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
}  // namespace erl::annotations