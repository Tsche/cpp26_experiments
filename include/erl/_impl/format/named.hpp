#include <experimental/meta>
#include <format>
#include <ranges>
#include <cstdio>
#include <source_location>
#include <tuple>
#include <utility>
#include <type_traits>
#include <vector>

#include <erl/_impl/util/meta.hpp>
#include <erl/_impl/util/containers.hpp>
#include <erl/_impl/util/parser.hpp>
#include <erl/_impl/util/string.hpp>
#include <erl/_impl/util/concepts.hpp>

#include <erl/_impl/compat/p1789.hpp>

namespace erl::impl::_format_impl {

struct Fragment {
  constexpr virtual ~Fragment() {}
  constexpr virtual void push(std::string&, int) = 0;
};

struct TextFragment final : Fragment {
  std::string data;

  consteval explicit TextFragment(std::string_view data) : data(data) {}
  constexpr ~TextFragment() override = default;

  constexpr void push(std::string& out, int) override { out += data; }
};

struct AccessorFragment final : Fragment {
  unsigned index;

  consteval explicit AccessorFragment(unsigned index) : index(index) {}
  constexpr ~AccessorFragment() override = default;
  constexpr void push(std::string& out, int direct_size) override {
    // clamp to 0
    auto value = std::max(direct_size + static_cast<int>(index), 0);

    out += util::utos(static_cast<unsigned>(value));
  }
};

struct Transformed {
  std::string fmt;
  std::meta::info direct_list = ^^void;
  std::meta::info accessors   = ^^void;
};

template <std::meta::info Member>
struct MemberAccessor {
  template <typename T>
  friend constexpr decltype(auto) operator>>(T&& obj, MemberAccessor) {
    return std::forward_like<T>(std::forward<T>(obj).[:Member:]);
  }
};

template <std::size_t Idx>
struct SubscriptAccessor {
  template <typename T>
  friend constexpr decltype(auto) operator>>(T&& obj, SubscriptAccessor) {
    return std::forward_like<T>(std::forward<T>(obj)[Idx]);
  }
};

template <std::size_t Idx>
struct TupleAccessor {
  template <typename T>
  friend constexpr decltype(auto) operator>>(T&& obj, TupleAccessor) {
    return std::get<Idx>(std::forward<T>(obj));
  }
};

template <std::size_t Idx, typename... Fields>
struct Accessor {
  static constexpr std::size_t index = Idx;

  template <typename T>
  static constexpr decltype(auto) get(T&& obj) {
    return (std::forward<T>(obj) >> ... >> Fields{});
  }
};

template <util::fixed_string fmt, typename DirectExpansions, typename Accessors, typename... Args>
std::string do_format(Args&&... args) {
  if constexpr (std::is_void_v<DirectExpansions> || std::is_void_v<Accessors>) {
    static_assert(false, std::string_view{fmt});
  } else {
    return [&]<std::size_t... Idx, typename... Accessor>(std::index_sequence<Idx...>,
                                                         util::TypeList<Accessor...>) {
      return std::format(fmt,
                         std::forward<Args...[Idx]>(args...[Idx])...,
                         Accessor::get(args...[Accessor::index])...);
    }(DirectExpansions{}, Accessors{});
  }
}

class FormatParser : util::Parser {
  std::source_location location;
  std::vector<std::unique_ptr<Fragment>> fragments;
  std::size_t fragment_start = 0;

  [[nodiscard]] static consteval std::meta::info get_accessor(std::string_view expression,
                                                              std::size_t index,
                                                              std::meta::info type) {
    std::vector accessors{std::meta::reflect_value(index)};
    for (auto subfield : std::views::split(expression, '.')) {
      type = remove_cvref(type);
      std::meta::info accessor{};

      if (util::is_integer(std::string_view{subfield})) {
        index = util::stou(std::string_view{subfield});
        if (meta::has_trait(^^util::is_subscriptable, type)) {
          // member access by subscript operator
          type     = substitute(^^util::subscript_result, {type});
          accessor = substitute(^^SubscriptAccessor, {std::meta::reflect_value(index)});
        } else if (meta::has_trait(^^util::is_tuple_like, type)) {
          // member access by get<Idx>(...)
          type     = substitute(^^std::tuple_element_t, {std::meta::reflect_value(index), type});
          accessor = substitute(^^TupleAccessor, {std::meta::reflect_value(index)});
        } else {
          // member access by index
          auto member = meta::get_nth_member(type, index);
          type        = type_of(member);
          accessor    = substitute(^^MemberAccessor, {reflect_value(member)});
        }
      } else {
        // member access by name
        index       = meta::get_member_index(type, std::string_view{subfield});
        auto member = meta::get_nth_member(type, index);
        type        = type_of(member);
        accessor    = substitute(^^MemberAccessor, {reflect_value(member)});
      }
      accessors.push_back(accessor);
    }

    return substitute(^^Accessor, accessors);
  }

  consteval Transformed error(std::string_view message) {
    auto line      = util::utos(location.line());
    auto column    = util::utos(location.column() - 1);
    auto error_str = std::string{"Invalid format string\n"};

    error_str += location.file_name();
    error_str += ':';
    error_str += line;
    error_str += ':';
    error_str += column;
    error_str += ": error: ";
    error_str += message;
    error_str += '\n';

    error_str += "  ";
    error_str += line;
    error_str += " | ";
    error_str += data;
    error_str += '\n';

    auto offset = 5 + line.size();
    error_str += std::string(offset, ' ');
    error_str += std::string(fragment_start - 1, ' ');
    error_str += '^';
    if (auto highlight = cursor - fragment_start + 1; highlight > 0) {
      error_str += std::string(highlight, '~');
    }
    error_str += '\n';
    error_str += "raised from:";

    return Transformed{error_str};
  }

  consteval auto push_text(std::string_view str = {}) {
    if (!str.empty()) {
      fragments.emplace_back(new TextFragment{str});
    } else {
      if (fragment_start == cursor) {
        return;
      }

      fragments.emplace_back(
          new TextFragment{data.substr(fragment_start, cursor - fragment_start)});
    }
    fragment_start = cursor;
  };

  consteval auto push_accessor(std::size_t index) {
    fragments.emplace_back(new AccessorFragment(index));
    fragment_start = cursor;
  };

  template <typename... Args>
  consteval Transformed do_parse() {
    auto [... idx] = std::index_sequence_for<Args...>();
    int direct[sizeof...(Args)]{((void)idx, -1)...};
    unsigned used = 0;

    std::vector<std::meta::info> accessors;
    std::meta::info arg_types[]{^^Args...};

    bool has_empty      = false;
    bool has_positional = false;

    while (is_valid()) {
      if (current() != '{') {
        ++cursor;
        continue;
      }

      ++cursor;

      // escaped curly braces
      if (current() == '{') {
        // skip to first unbalanced } - this will match the outer {
        skip_to('}');
        push_text();
        continue;
      }

      push_text();

      // find name
      auto start = cursor;
      skip_to('}', ':');
      if (start == cursor) {
        // no name
        has_empty = true;
        if (has_positional) {
          return error("Either all or no arguments can be positional");
        }

        ++cursor;
        push_text();
        continue;
      } else {
        has_positional = true;
        if (has_empty) {
          return error("Either all or no arguments can be positional");
        }
      }

      auto name    = data.substr(start, cursor - start);
      auto dot_pos = name.find('.');

      unsigned index = 0;
      if (util::is_integer(name)) {
        // by-index field, ie. {0}
        index = util::stou(name);
        if (direct[index] == -1) {
          direct[index] = used++;
        }
        push_text(util::utos(direct[index]));
        continue;
      } else if (dot_pos == name.npos || !util::is_digit(name[0])) {
        // just a name -> implicit field 0
        if constexpr (sizeof...(Args) != 1) {
          return error("Field names cannot be directly used if there's more than one argument");
        }
        dot_pos = 0;
      } else {
        auto arg_index = name.substr(0, dot_pos);
        index          = util::stou(arg_index);
        ++dot_pos;
      }

      accessors.push_back(get_accessor(name.substr(dot_pos), index, arg_types[index]));
      push_accessor(accessors.size());
      ++cursor;
    }
    push_text();

    std::vector<std::meta::info> direct_indices;
    if (has_positional) {
      direct_indices = std::vector<std::size_t>{idx...} |
                       std::views::filter([&](auto x) { return direct[x] != -1; }) |
                       std::views::transform([](auto x) { return std::meta::reflect_value(x); }) |
                       std::ranges::to<std::vector>();
    } else {
      direct_indices = {std::meta::reflect_value(idx)...};
    }

    return Transformed{render(static_cast<int>(direct_indices.size()) - 1),
                       substitute(^^std::index_sequence, direct_indices),
                       substitute(^^util::TypeList, accessors)};
  }

public:
  consteval FormatParser(std::string_view source, std::source_location const& location)
      : util::Parser(source)
      , location(location) {}

  [[nodiscard]] constexpr std::string render(int accessor_offset) const {
    auto out = std::string{};
    for (auto&& fragment : fragments) {
      fragment->push(out, accessor_offset);
    }
    return out;
  }

  template <typename... Args>
  static consteval auto parse(std::string_view source, std::source_location const& location) {
    auto parser = FormatParser(source, location);
    return parser.do_parse<Args...>();
  }
};

}  // namespace erl::impl::_format_impl