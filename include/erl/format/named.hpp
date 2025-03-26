#include <experimental/meta>
#include <format>
#include <ranges>
#include <cstdio>
#include <utility>
#include <type_traits>

#include <erl/util/meta.hpp>
#include <erl/util/containers.hpp>
#include <erl/util/parser.hpp>
#include <erl/util/string.hpp>

#include <erl/compat/p1789.hpp>

namespace erl::formatting {

template <std::size_t Idx, std::meta::info... Fields>
struct Accessor {
  static constexpr std::size_t index = Idx;

  template <typename T>
  static constexpr decltype(auto) get(T&& obj) {
    return (std::forward<T>(obj).*....*&[:Fields:]);
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

struct Fragment {
  constexpr virtual ~Fragment() {}
  constexpr virtual void push(std::string&, int) = 0;
};

struct TextFragment final : Fragment {
  std::string data;

  consteval TextFragment(std::string_view data) : data(data) {}
  constexpr ~TextFragment() override = default;

  constexpr void push(std::string& out, int) override { out += data; }
};

struct AccessorFragment final : Fragment {
  unsigned index;

  consteval AccessorFragment(unsigned index) : index(index) {}
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

struct FormatString : util::Parser {
  template <typename... Args>
  consteval Transformed transform(std::source_location const& location) {
    auto [... idx] = std::index_sequence_for<Args...>();
    int direct[sizeof...(Args)]{((void)idx, -1)...};
    unsigned used = 0;

    std::meta::info arg_types[]{^^Args...};

    std::vector<std::meta::info> accessors;
    std::vector<std::unique_ptr<Fragment>> fragments;
    std::size_t fragment_start = 0;

    auto push_text = [&](std::string str = "") {
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

    auto error = [&](std::string_view message) {
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
    };

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
        if (has_empty && has_positional) {
          return error("Either all or no arguments can be positional");
        }
        ++cursor;
        push_text();
        continue;
      } else {
        has_positional = true;
        if (has_empty && has_positional) {
          return error("Either all or no arguments can be positional");
        }
      }

      auto name    = data.substr(start, cursor - start);
      auto dot_pos = name.find('.');

      unsigned index = 0;
      if (dot_pos == name.npos) {
        if (util::is_integer(name)) {
          // by-index field, ie. {0}
          index = util::stou(name);
          if (direct[index] == -1) {
            direct[index] = used++;
          }
          push_text(util::utos(direct[index]));
          continue;
        } else {
          // just a name -> implicit field 0
          if constexpr (sizeof...(Args) != 1) {
            return error("Field names cannot be directly used if there's more than one argument");
          }
          dot_pos = 0;
        }
      } else {
        auto arg_index = name.substr(0, dot_pos);
        index          = util::stou(arg_index);
        ++dot_pos;
      }

      std::vector indices{std::meta::reflect_value(index)};

      auto field       = name.substr(dot_pos);
      auto field_index = 0;
      auto member      = std::meta::info{};
      auto type        = arg_types[index];

      for (auto subfield : std::views::split(field, '.')) {
        field_index = extract<std::size_t (*)(std::string_view)>(
            substitute(^^meta::get_member_index, {type}))(std::string_view{subfield});
        member = meta::get_nth_member(type, field_index);
        type   = type_of(member);

        indices.push_back(std::meta::reflect_value(member));
      }

      accessors.push_back(substitute(^^Accessor, indices));

      fragments.emplace_back(new AccessorFragment(accessors.size()));
      fragment_start = cursor;
      ++cursor;
    }
    push_text();

    std::vector<std::meta::info> direct_indices;
    if (has_positional) {
      direct_indices = std::vector{idx...} |
                       std::views::filter([&](auto x) { return direct[x] != -1; }) |
                       std::views::transform([](auto x) { return std::meta::reflect_value(x); }) |
                       std::ranges::to<std::vector>();
    } else {
      direct_indices = {std::meta::reflect_value(idx)...};
    }

    return Transformed{render(fragments, static_cast<int>(direct_indices.size()) - 1),
                       substitute(^^std::index_sequence, direct_indices),
                       substitute(^^util::TypeList, accessors)};
  }

  [[nodiscard]] static constexpr std::string render(
      std::vector<std::unique_ptr<Fragment>> const& fragments,
      int accessor_offset) {
    auto out = std::string{};
    for (auto&& fragment : fragments) {
      fragment->push(out, accessor_offset);
    }
    return out;
  }
};

template <typename... Args>
struct NamedFormatString {
  using format_type = std::string (*)(Args&&...);
  format_type handle;

  template <typename Tp>
    requires std::convertible_to<Tp const&, std::string_view>
  consteval explicit(false)
      NamedFormatString(Tp const& str,
                        std::source_location const& location = std::source_location::current()) {
    auto parser                        = FormatString{str};
    auto [fmt, direct_list, accessors] = parser.transform<std::remove_cvref_t<Args>...>(location);

    handle = extract<format_type>(
        substitute(^^do_format, {meta::promote(fmt), direct_list, accessors, ^^Args...}));
  }
};
}  // namespace erl::formatting