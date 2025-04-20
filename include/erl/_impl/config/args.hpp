#pragma once

#include <experimental/meta>
#include <cstdio>
#include <print>
#include <ranges>
#include <utility>
#include <filesystem>

#include "default_construct.hpp"
#include "annotations.hpp"
#include "erl/_impl/util/concepts.hpp"

#include <erl/_impl/compat/p1789.hpp>
#include <erl/_impl/expect.hpp>

#include <erl/_impl/util/string.hpp>
#include <erl/_impl/util/meta.hpp>
#include <erl/_impl/log/format/color.hpp>

#include <erl/_impl/compile_error.hpp>

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
      if constexpr (util::is_nonstatic_member_function<R>) {
        default_invoke<R>(*static_cast<[:parent_of(R):]*>(obj), arg_tuple);
      } else if constexpr (is_function(R)) {
        default_invoke<R>(arg_tuple);
      } else if constexpr (is_object_type(type_of(R))) {
        static_cast<[:parent_of(R):]*>(obj)->[:R:] = *get<0>(arg_tuple);
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
      ++parser.cursor; // parser will not unwind after this point
      
      bool contd = true;
      [:meta::sequence(sizeof...(args)):] >>= [&]<auto Idx>{
        if (!contd) {
          return;
        }

        Argument::Unevaluated argument;
        if ((argument.*args...[Idx].parse)(parser)) {
          arguments.push_back(argument);
        } else {
          if (!args...[Idx].optional) {
            parser.fail("Missing argument {} of option {}", args...[Idx].name, arg_name);
            contd = false;
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

struct Spec {
  struct Parser {
    std::vector<Argument> arguments;
    std::vector<Option> commands;
    std::vector<Option> options;
    // subspec for subcommands?

    consteval void parse(std::meta::info r) {
      for (auto member : members_of(r)) {
        if (!is_public(member) || !has_identifier(member)) {
          continue;
        }
        (void)(parse_argument(member) || parse_command(member) || parse_option(member));
      }
    }

    consteval void parse_base(std::meta::info r) {
      if (meta::is_derived_from(r, ^^CLI)) {
        for (auto fnc_template :
             members_of(^^CLI) | std::views::filter(std::meta::is_function_template)) {
          if (!can_substitute(fnc_template, {r})) {
            continue;
          }

          auto fnc = substitute(fnc_template, {r});
          if (meta::has_annotation<annotations::Option>(fnc)) {
            commands.emplace_back(identifier_of(fnc_template), fnc);
          }
        }
      }
    }

    consteval bool parse_argument(std::meta::info r) {
      if (!is_nonstatic_data_member(r)) {
        return false;
      }

      if (meta::has_annotation(r, ^^annotations::Option) ||
          meta::has_annotation(r, ^^annotations::Descend)) {
        return false;
      }

      arguments.emplace_back(arguments.size(), r);
      return true;
    }

    consteval bool parse_command(std::meta::info r) {
      if (is_function(r) && is_static_member(r)) {
        if (meta::has_annotation(r, ^^annotations::Option)) {
          commands.emplace_back(identifier_of(r), r);
          return true;
        }
      }

      return false;
    }

    consteval bool parse_option(std::meta::info r) {
      if (!meta::has_annotation(r, ^^annotations::Option)) {
        return false;
      }

      if (is_nonstatic_data_member(r)) {
        if (!has_default_member_initializer(r)) {
          compile_error(std::string("Option ") + identifier_of(r) + " lacks a default member initializer.");
        }
      } else if (!is_function(r) || is_static_member(r)){
        return false;
      }

      options.emplace_back(identifier_of(r), r);
      return true;
    }
    };

    std::span<Argument const> arguments;
    std::span<Option const> commands;
    std::span<Option const> options;

    consteval explicit Spec(std::meta::info r) {
      auto parser = Parser();
      parser.parse(r);
      parser.parse_base(r);
      arguments = define_static_array(parser.arguments);
      commands  = define_static_array(parser.commands);
      options   = define_static_array(parser.options);
    }
  };

  template <typename T>
  T parse_args(std::vector<std::string_view> args_in) {
    static constexpr Spec spec{^^T};
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