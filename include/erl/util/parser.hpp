#pragma once
#include <string_view>
#include <cstddef>

namespace erl::util {

constexpr bool is_whitespace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

struct Parser {
  std::string_view data;
  std::size_t cursor{0};

  constexpr void skip_whitespace() {
    while (is_valid()) {
      if (char c = current(); is_whitespace(c) || c == '\\') {
        ++cursor;
      } else {
        break;
      }
    }
  }

  constexpr void skip_to(std::same_as<char> auto... needles) {
    int brace_count = 0;
    while (is_valid()) {
      if (char c = current(); brace_count == 0 && ((c == needles) || ...)) {
        break;
      } else if (c == '[' || c == '{' || c == '(') {
        ++brace_count;
      } else if (c == ']' || c == '}' || c == ')') {
        --brace_count;
      }
      ++cursor;
    }
  }

  [[nodiscard]] constexpr char current() const { return data[cursor]; }
  [[nodiscard]] constexpr bool is_valid() const { return cursor < data.length(); }
};
}  // namespace util