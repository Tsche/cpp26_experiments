#pragma once

#include <experimental/meta>
#include <cstdio>
#include <print>
#include <ranges>
#include <utility>
#include <filesystem>

#include "default_construct.hpp"
#include "annotations.hpp"
#include <erl/_impl/expect.hpp>

#include <erl/_impl/util/string.hpp>
#include <erl/_impl/util/meta.hpp>
#include <erl/_impl/log/format/color.hpp>

#include <erl/info>

namespace erl::_impl {
template <typename T>
T parse_value(std::string_view value) {
  // TODO parse more types
  if constexpr (std::is_integral_v<T>) {
    // TODO parse signed, bool etc
    return util::stou(value);
  } else {
    return T{value};
  }
}

struct ArgParser {
  std::span<std::string_view> args;
  std::size_t cursor = 0;

  std::string_view current() const { return args[cursor]; }

  template <typename... Args>
  [[noreturn]] void fail(std::format_string<Args...> fmt, Args&&... args) const {
    std::println(fmt, std::forward<Args>(args)...);
    std::exit(1);
  }

  [[nodiscard]] bool valid() const noexcept { return cursor < args.size(); }
};

struct Argument {
  struct Unevaluated {
    using handler_type = void (Unevaluated::*)(void*) const;

    std::string_view argument;
    handler_type handle = nullptr;

    void operator()(void* args) const { (this->*handle)(args); }

    template <std::meta::info R, typename T>
    void validate(T value) const {
      auto parent = display_string_of(parent_of(R));
      if constexpr (erl::meta::has_annotation(R, ^^erl::annotations::Expect)) {
        constexpr static auto constraint = [:value_of(erl::meta::get_annotation(
                                                 R,
                                                 ^^erl::annotations::Expect)):];

        std::vector<std::string> failed_terms;
        if (constraint.eval_verbose(erl::Tuple{value}, failed_terms)) {
          return;
        }

        std::println("Validation failed for argument `{}` of `{}`", identifier_of(R), parent);
        for (auto&& term : failed_terms | std::views::reverse) {
          std::println("    => {}{}{} evaluated to {}false{}",
                       fg::Blue,
                       term,
                       style::Reset,
                       fg::Red,
                       style::Reset);
        }
        std::exit(1);
      }
    }

    template <std::size_t Idx, std::meta::info R>
    void do_handle(void* arguments) const {
      using parent    = [:is_function_parameter(R) ? type_of(parent_of(R)) : parent_of(R):];
      using arg_tuple = ArgumentTuple<parent>;

      using type = [:remove_cvref(type_of(R)):];
      auto value = parse_value<type>(argument);
      validate<R>(value);
      get<Idx>(*static_cast<arg_tuple*>(arguments)) = value;
    }

    template <std::size_t Idx, std::meta::info R>
    bool do_parse(ArgParser& parser) {
      static constexpr auto current_handler =
          meta::instantiate<handler_type>(^^do_handle,
                                          std::meta::reflect_value(Idx),
                                          reflect_value(R));

      if (!parser.valid()) {
        return false;
      }

      auto current = parser.current();
      if (current[0] == '-') {
        if (current.size() > 1 && current[1] >= '0' && current[1] <= '9') {
          ++parser.cursor;
          return true;
        }
        return false;
      }

      argument = current;
      handle   = current_handler;
      ++parser.cursor;
      return true;
    }
  };

  using parser_type = bool (Unevaluated::*)(ArgParser&);
  std::size_t index;
  parser_type parse;

  char const* name;
  char const* type;
  char const* description;
  bool optional = false;
  char const* constraint;

  template <std::meta::info R, auto Constraint>
  constexpr char const* stringify_constraint() {
    std::string _constraint = Constraint.to_string(identifier_of(R));
    return std::define_static_string(_constraint.substr(1, _constraint.size() - 2));
  }

  consteval Argument(std::size_t idx, std::meta::info reflection)
      : index(idx)
      , parse(meta::instantiate<parser_type>(^^Unevaluated::template do_parse,
                                             std::meta::reflect_value(idx),
                                             reflect_value(reflection)))
      , name(std::define_static_string(identifier_of(reflection)))
      , type(std::define_static_string(
            display_string_of(type_of(reflection))))  // TODO clean at this point?
      , optional(is_function_parameter(reflection) ? has_default_argument(reflection)
                                                   : has_default_member_initializer(reflection)) {
    if (auto desc = annotation_of_type<annotations::Description>(reflection); desc) {
      description = desc->data;
    } else {
      description = std::define_static_string("");
    }

    if (erl::meta::has_annotation(reflection, ^^erl::annotations::Expect)) {
      auto annotation = erl::meta::get_annotation(reflection, ^^erl::annotations::Expect);
      constraint = (this->*meta::instantiate<char const* (Argument::*)()>(^^stringify_constraint,
                                                                          reflect_value(reflection),
                                                                          value_of(annotation)))();
    } else {
      constraint = nullptr;
    }
  }
};

struct Option {
  struct Unevaluated {
    using handler_type = void (Unevaluated::*)(void*) const;
    std::vector<Argument::Unevaluated> arguments;
    handler_type handle;
    void operator()(void* object) const { (this->*handle)(object); }

    template <std::meta::info R>
    void do_handle(void* obj) const {
      ArgumentTuple<[:type_of(R):]> arg_tuple;
      for (auto arg : arguments) {
        arg(&arg_tuple);
      }

      if constexpr (is_function(R)) {
        default_invoke<required_arg_count<R>, [:type_of(R):]>(
            [&]<typename... Args>(Args&&... args) {
              if constexpr (is_static_member(R)) {
                [:R:](std::forward<Args>(args)...);
              } else {
                static_cast<[:parent_of(R):]*>(obj) -> [:R:](std::forward<Args>(args)...);
              }
            },
            arg_tuple);
      } else if constexpr (is_object_type(type_of(R))) {
        default_invoke<required_arg_count<R>, [:type_of(R):]>(
            [&]<typename Arg>(Arg&& arg) {
              static_cast<[:parent_of(R):]*>(obj) -> [:R:] = std::forward<Arg>(arg);
            },
            arg_tuple);
      } else {
        static_assert(false, "Unsupported handler type.");
      }
    }

    template <util::fixed_string name, std::meta::info R, Argument... args>
    bool do_parse(ArgParser& parser) {
      static constexpr auto current_handler =
          meta::instantiate<handler_type>(^^do_handle, reflect_value(R));
      auto arg_name = parser.current();
      arg_name.remove_prefix(2);
      if (arg_name != name) {
        return false;
      }
      ++parser.cursor;

      [:meta::sequence(sizeof...(args)):] >>= [&]<auto Idx> {
        Argument::Unevaluated argument;
        if ((argument.*args...[Idx].parse)(parser)) {
          arguments.push_back(argument);
        } else {
          if (!args...[Idx].optional) {
            parser.fail("Missing argument {} of option {}", args...[Idx].name, arg_name);
          }
        }
      };

      handle = current_handler;
      return true;
    }
  };

  using parser_type = bool (Unevaluated::*)(ArgParser&);

  parser_type parse;
  char const* name;
  char const* description = nullptr;

  std::size_t argument_count;
  Argument const* arguments;

  consteval Option(std::string_view name, std::meta::info reflection)
      : name(std::define_static_string(name)) {
    if (auto desc = annotation_of_type<annotations::Description>(reflection); desc) {
      description = desc->data;
    }
    std::vector<Argument> args;
    std::size_t index = 0;
    if (is_object_type(type_of(reflection))) {
      args.emplace_back(index, reflection);
    } else {
      for (auto param : parameters_of(reflection)) {
        args.emplace_back(index++, param);
      }
    }

    argument_count = args.size();
    arguments      = std::define_static_array(args).data();

    std::vector parse_args = {meta::promote(name), reflect_value(reflection)};
    for (auto arg : args) {
      parse_args.push_back(std::meta::reflect_value(arg));
    }

    parse = extract<parser_type>(substitute(^^Unevaluated::template do_parse, parse_args));
  }
};

struct CLI;

template <typename T>
  requires(std::is_aggregate_v<T> && !std::is_array_v<T>)
class Spec {
private:
  consteval auto get_arguments() {
    std::vector<Argument> fields;
    std::size_t idx = 0;
    for (auto member : nonstatic_data_members_of(^^T)) {
      // TODO handle descend
      if (!meta::has_annotation<annotations::Option>(member)) {
        fields.emplace_back(idx++, member);
      }
    }
    return fields;
  }

  consteval auto get_commands() {
    std::vector<Option> fields;

    for (auto static_fnc : meta::static_member_functions_of(^^T)) {
      if (meta::has_annotation<annotations::Option>(static_fnc)) {
        fields.emplace_back(identifier_of(static_fnc), static_fnc);
      }
    }

    if constexpr (std::derived_from<T, CLI>) {
      for (auto fnc_template :
           members_of(^^CLI) | std::views::filter(std::meta::is_function_template)) {
        if (!can_substitute(fnc_template, {^^T})) {
          continue;
        }

        auto fnc = substitute(fnc_template, {^^T});
        if (meta::has_annotation<annotations::Option>(fnc)) {
          fields.emplace_back(identifier_of(fnc_template), fnc);
        }
      }
    }
    return fields;
  }

  consteval auto get_options() {
    std::vector<Option> fields;
    for (auto member : nonstatic_data_members_of(^^T)) {
      if (meta::has_annotation<annotations::Option>(member) &&
          has_default_member_initializer(member)) {
        fields.emplace_back(identifier_of(member), member);
      }
    }

    for (auto fnc : meta::nonstatic_member_functions_of(^^T)) {
      if (meta::has_annotation<annotations::Option>(fnc)) {
        fields.emplace_back(identifier_of(fnc), fnc);
      }
    }
    return fields;
  }

public:
  std::span<Argument const> arguments;  // arguments required or usable to construct T
  std::span<Option const> commands;  // options that do not require a T
  std::span<Option const> options;   // regular options

  consteval Spec()
      : arguments(std::define_static_array(get_arguments()))
      , commands(std::define_static_array(get_commands()))
      , options(std::define_static_array(get_options())) {}
};

template <typename T>
T parse_args(std::vector<std::string_view> args_in) {
  static constexpr Spec<T> spec{};
  auto parser          = ArgParser{args_in};
  auto& [args, cursor] = parser;

  Program::set_name(args[0]);
  cursor = 1;

  std::vector<Argument::Unevaluated> parsed_args{};
  for (auto Arg : spec.arguments) {
    Argument::Unevaluated argument{};
    if ((argument.*Arg.parse)(parser)) {
      parsed_args.push_back(argument);
    } else {
      break;
    }
  }

  std::vector<typename Option::Unevaluated> parsed_opts{};
  auto [... Cmds] = [:meta::expand(spec.commands):];
  auto [... Opts] = [:meta::expand(spec.options):];

  while (parser.valid()) {
    bool found = false;
    typename Option::Unevaluated option{};

    if (((option.*Cmds.parse)(parser) || ...)) {
      // run commands right away
      option(nullptr);
      continue;
    }

    found = ((option.*Opts.parse)(parser) || ...);
    if (!found) {
      parser.fail("Could not find option `{}`", parser.current());
    }
    parsed_opts.push_back(option);
  }

  ArgumentTuple<T> args_tuple;
  for (auto argument : parsed_args) {
    argument(&args_tuple);
  }

  for (auto arg : spec.arguments | std::views::drop(parsed_args.size())) {
    if (!arg.optional) {
      parser.fail("Missing required argument `{}`", arg.name);
    }
  }

  T object = default_construct<T>(args_tuple);

  // run options
  for (auto&& option : parsed_opts) {
    option(&object);
  }

  return object;
}
}  // namespace erl::_impl