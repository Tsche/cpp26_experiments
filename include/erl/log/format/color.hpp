#pragma once
#include <format>
#include <string_view>
#include <string>
#include <erl/util/string.hpp>

namespace erl::color {
constexpr std::string make_ansi_code(unsigned option) {
  return "\x1B[" + util::utos(option) + "m";
}

constexpr std::string make_ansi_code(std::convertible_to<unsigned> auto... options) {
  std::string ret    = "\x1B[";
  bool must_separate = false;
  for (auto option : {options...}) {
    if (must_separate) {
      ret += ";";
    } else {
      must_separate = true;
    }

    ret += util::utos(option);
  }
  ret += "m";
  return ret;
}

template <unsigned... Codes>
struct ANSICodes;

template <unsigned Code>
struct ANSICode {
  static constexpr auto code  = Code;
  static constexpr auto value = [:meta::intern(make_ansi_code(Code)):];

  template <unsigned N>
  friend auto operator|(ANSICode self, ANSICode<N> const&) {
    return ANSICodes<Code, N>{};
  }

  template <unsigned... N>
  friend auto operator|(ANSICode self, ANSICodes<N...> const&) {
    return ANSICodes<Code, N...>{};
  }

  constexpr explicit operator unsigned() const { return Code; }
  constexpr operator std::string_view() const { return value; }

  template <typename T>
  constexpr std::string operator()(T&& str) const {
    if constexpr (Code == 0) {
      return std::string(str);
    } else {
      return std::format("{}{}{}", *this, std::forward<T>(str), ANSICode<0>{});
    }
  }
};

template <unsigned... Codes>
struct ANSICodes {
  static constexpr auto value = [:meta::intern(make_ansi_code(Codes...)):];

  template <unsigned N>
  friend auto operator|(ANSICodes, ANSICode<N> const&) {
    return ANSICodes<Codes..., N>{};
  }

  template <unsigned... N>
  friend auto operator|(ANSICodes, ANSICodes<N...> const&) {
    return ANSICodes<Codes..., N...>{};
  }

  constexpr operator std::string_view() const { return value; }

  template <typename T>
  constexpr std::string operator()(T&& str) const {
    return std::format("{}{}{}", *this, std::forward<T>(str), ANSICode<0>{});
  }
};
}  // namespace erl::color

template <unsigned Code>
struct std::formatter<erl::color::ANSICode<Code>> : std::formatter<std::string> {
  auto format(erl::color::ANSICode<Code> const& code, std::format_context& ctx) const {
    return std::formatter<std::string>::format(std::string(code), ctx);
  }
};

template <unsigned... Codes>
struct std::formatter<erl::color::ANSICodes<Codes...>> : std::formatter<std::string> {
  auto format(erl::color::ANSICodes<Codes...> const& code, std::format_context& ctx) const {
    return std::formatter<std::string>::format(std::string(code), ctx);
  }
};

namespace erl {
namespace fg {
inline constexpr color::ANSICode<30> Black{};
inline constexpr color::ANSICode<31> Red{};
inline constexpr color::ANSICode<32> Green{};
inline constexpr color::ANSICode<33> Yellow{};
inline constexpr color::ANSICode<34> Blue{};
inline constexpr color::ANSICode<35> Magenta{};
inline constexpr color::ANSICode<36> Cyan{};
inline constexpr color::ANSICode<37> White{};
inline constexpr color::ANSICode<39> Default{};
};  // namespace fg

namespace bg {
inline constexpr color::ANSICode<40> Black{};
inline constexpr color::ANSICode<41> Red{};
inline constexpr color::ANSICode<42> Green{};
inline constexpr color::ANSICode<43> Yellow{};
inline constexpr color::ANSICode<44> Blue{};
inline constexpr color::ANSICode<45> Magenta{};
inline constexpr color::ANSICode<46> Cyan{};
inline constexpr color::ANSICode<47> White{};
inline constexpr color::ANSICode<49> Default{};
};  // namespace bg

namespace style {
inline constexpr color::ANSICode<1> Bold{};
inline constexpr color::ANSICode<2> Dim{};
inline constexpr color::ANSICode<3> Italic{};
inline constexpr color::ANSICode<4> Underlined{};
inline constexpr color::ANSICode<5> Blinking{};
inline constexpr color::ANSICode<7> Inverse{};
inline constexpr color::ANSICode<8> Hidden{};
inline constexpr color::ANSICode<9> Strikethrough{};
inline constexpr color::ANSICode<0> Reset{};
};  // namespace style
}  // namespace erl