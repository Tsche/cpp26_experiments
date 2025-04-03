#pragma once
#include <algorithm>
#include <string_view>
#include <string>
#include <cstddef>
#include <vector>
#include <span>
#include <utility>
#include <iterator>
#include <experimental/meta>

#include "string.hpp"
#include "meta.hpp"

namespace erl::util {

namespace _trie_impl {
template <auto... Transitions>
struct State {
  char const* transition;
  std::size_t size;
  unsigned word_index;

  [[clang::always_inline]]
  bool visit(std::string_view str, auto&& visitor) const {
    if (size == 1) {
      if (word_index) {
        if (str.size() != 1) {
          return false;
        }
      }
      if (!(str[0] == transition[0])) {
        return false;
      }

    } else {
      if (!str.starts_with(transition)) {
        return false;
      }
    }
    str.remove_prefix(size);
    if (str.empty()) {
      // word index is offset by 1
      // => index 0 means no match
      std::forward<decltype(visitor)>(visitor)(word_index);
      return bool(word_index);
    }
    return (Transitions.visit(str, std::forward<decltype(visitor)>(visitor)) || ...);
  }
};

template <auto... Transitions>
struct Root {
  static bool matches(std::string_view str) {
    if (str.empty()) {
      return false;
    }
    // return (Transitions.matches(str) || ...);
    return (Transitions.visit(str, [](unsigned v) {}) || ...);
  }

  static unsigned find(std::string_view str) {
    unsigned result = -1U;
    if (str.empty()) {
      return result;
    }
    (void)(Transitions.visit(str, [&](unsigned v) { result = v; }) || ...);
    return result - 1;
  }
};

template <auto... Transitions>
constexpr inline Root<Transitions...> make_trie{};

template <fixed_string transition, unsigned word_index, auto... Transitions>
constexpr inline auto make_state =
    State<Transitions...>{transition.data, transition.size, word_index};

struct ParsedState {
  std::string prefix;
  std::vector<ParsedState> transitions;
  unsigned word_index = 0;

  static constexpr ParsedState make(std::span<std::string_view> words) {
    std::vector<std::string_view> sorted_words{};
    sorted_words.assign_range(words);
    std::ranges::sort(sorted_words);

    ParsedState root{"", {}};

    for (auto word : sorted_words) {
      ParsedState* node = &root;
      std::size_t i     = 0;

      while (i < word.size()) {
        auto it = std::ranges::find_if(node->transitions, [&](const ParsedState& s) {
          return s.prefix.starts_with(word[i]);
        });

        if (it == node->transitions.end()) {
          unsigned word_idx =
              std::ranges::distance(words.begin(), std::ranges::find(words, word)) + 1;

          node->transitions.emplace_back(std::string(word.substr(i)),
                                         std::vector<ParsedState>{
                                             {"", {}, word_idx}
          });
          break;
        }

        std::size_t const match_len =
            std::mismatch(it->prefix.begin(), it->prefix.end(), word.begin() + i).first -
            it->prefix.begin();

        if (match_len < it->prefix.size()) {
          ParsedState split_node{it->prefix.substr(match_len), std::move(it->transitions)};
          it->prefix.resize(match_len);
          it->transitions = {std::move(split_node)};
        }

        i += match_len;
        node = &(*it);
      }
    }
    return root;
  }

  explicit(false) consteval operator std::meta::info() const {
    std::vector<std::meta::info> states = {};
    // bool is_leaf = transitions.empty();
    unsigned index = 0;
    for (auto&& state : transitions) {
      if (state.prefix.empty()) {
        // is_leaf = true;
        index = state.word_index;
        continue;
      }
      states.push_back(state);
    }

    if (!transitions.empty() && prefix.empty()) {
      // prevent aggressive inlining at the root level
      return substitute(^^make_trie, states);
    }

    std::vector args = {meta::promote(prefix), std::meta::reflect_value(index)};
    for (auto state : states) {
      args.push_back(state);
    }
    return substitute(^^make_state, args);
  }
};
}  // namespace _trie_impl
struct Trie {
  bool (*matches)(std::string_view str)  = nullptr;
  unsigned (*find)(std::string_view str) = nullptr;

  explicit consteval Trie(std::vector<std::string_view> words) {
    // erase tree type
    auto tree = _trie_impl::ParsedState::make(words);
    (this->*extract<void (Trie::*)()>(substitute(^^assign_handlers, {tree})))();
  }

private:
  template <auto Parser>
  constexpr void assign_handlers() {
    matches = &Parser.matches;
    find    = &Parser.find;
  }
};
}  // namespace erl::util