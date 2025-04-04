#pragma once

#include <experimental/meta>
#include <cstdio>
#include <print>
#include <ranges>
#include <utility>
#include <filesystem>

#include "default_construct.hpp"
#include "annotations.hpp"
#include "expect.hpp"

#include <erl/util/string.hpp>
#include <erl/util/meta.hpp>
#include <erl/log/format/color.hpp>

namespace erl {
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

  bool valid() const { return cursor < args.size(); }
};

template <typename T, std::size_t Idx>
void check_default_initializable(ArgParser const& parser, ArgumentTuple<T> const& arguments) {
  constexpr static auto member = nonstatic_data_members_of(^^T)[Idx];
  if constexpr (!has_default_member_initializer(member)) {
    parser.fail("Missing required argument: {}", identifier_of(member));
  }
}

template <std::meta::info R, typename T>
void validate(T value, std::string_view parent) {
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

struct ArgumentInfo {
  char const* name;
  char const* type;
  char const* description;
  bool optional = false;
  std::size_t constraint_count;
  char const* const* constraints;

  explicit consteval ArgumentInfo(std::meta::info reflection)
      : name(std::meta::define_static_string(identifier_of(reflection)))
      , type(std::meta::define_static_string(
            display_string_of(type_of(reflection))))  // TODO clean at this point?
      , optional(is_function_parameter(reflection) ? has_default_argument(reflection)
                                                   : has_default_member_initializer(reflection)) {
    if (auto desc = annotation_of_type<annotations::Description>(reflection); desc) {
      description = desc->data;
    } else {
      description = std::meta::define_static_string("");
    }

    constraint_count = 0;
    constraints      = nullptr;  // std::meta::define_static_array(std::vector{""}).data();
  }
};

struct Argument {
  struct Unevaluated {
    using handler_type = void (Unevaluated::*)(void*) const;

    std::string_view argument;
    handler_type handle = nullptr;

    void operator()(void* args) const { (this->*handle)(args); }

    template <std::size_t Idx, std::meta::info R>
    void do_handle(void* arguments) const {
      using parent    = [:is_function_parameter(R) ? type_of(parent_of(R)) : parent_of(R):];
      using arg_tuple = ArgumentTuple<parent>;

      using type = [:remove_cvref(type_of(R)):];
      auto value = parse_value<type>(argument);
      validate<R>(value, display_string_of(parent_of(R)));
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
  // using default_type = void (*)(ArgParser const&, ArgumentTuple<T> const&);

  std::size_t index;
  parser_type parse;
  // default_type check_default;
  ArgumentInfo info;

  consteval Argument(std::size_t idx, std::meta::info reflection)
      : index(idx)
      , parse(meta::instantiate<parser_type>(^^Unevaluated::template do_parse,
                                             std::meta::reflect_value(idx),
                                             reflect_value(reflection)))
      // , check_default(meta::instantiate<default_type>(^^check_default_initializable,
      // ^^T,
      // std::meta::reflect_value(idx)))
      , info(reflection) {}
};

template <typename T>
struct Option {
  struct Unevaluated {
    using handler_type = void (Unevaluated::*)(T*) const;
    std::vector<Argument::Unevaluated> arguments;
    handler_type handle;
    void operator()(T* object) const { (this->*handle)(object); }

    template <std::meta::info R>
    void do_handle(T* obj) const {
      // TODO REFACTOR
      if constexpr (parent_of(R) == ^^T) {
        if constexpr (is_object_type(type_of(R))) {
          ArgumentTuple<[:type_of(R):]> arg_tuple;
          for (auto arg : arguments) {
            arg(&arg_tuple);
          }
          default_invoke<required_arg_count<R>, [:type_of(R):]>([&]<typename Arg>(Arg&& arg){
            obj->[:R:] = std::forward<Arg>(arg);
          }, arg_tuple);
        } else {
          ArgumentTuple<[:type_of(R):]> arg_tuple;
          for (auto arg : arguments) {
            arg(&arg_tuple);
          }
          default_invoke<required_arg_count<R>, [:type_of(R):]>([&]<typename... Args>(Args&&... args){
            obj->[:R:](std::forward<Args>(args)...);
          }, arg_tuple);
        }
      } else {
        // TODO instead check if this is a static function
        ArgumentTuple<[:type_of(parent_of(R)):]> arg_tuple;
        for (auto arg : arguments) {
          arg(&arg_tuple);
        }
        default_invoke<required_arg_count<R>>([]<typename... Args>(Args&&... args){
          [:R:](std::forward<Args>(args)...);
        }, arg_tuple);
      }
    }

    template <util::fixed_string name, std::meta::info R, Argument... args>
    bool do_parse(ArgParser& parser) {
      static constexpr auto current_handler = meta::instantiate<handler_type>(^^do_handle, reflect_value(R));
      auto arg_name                         = parser.current();
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
          if (!args...[Idx].info.optional){
            parser.fail("Missing argument {} of option {}", args...[Idx].info.name, arg_name);
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
  ArgumentInfo const* arguments;

  consteval Option(std::string_view name, std::meta::info reflection)
      : name(std::meta::define_static_string(name)) {
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

    std::vector<ArgumentInfo> arg_infos;
    for (auto arg : args) {
      arg_infos.push_back(arg.info);
    }

    argument_count = args.size();
    arguments      = std::meta::define_static_array(arg_infos).data();

    std::vector parse_args = {meta::promote(name), reflect_value(reflection)};
    for (auto arg : args) {
      parse_args.push_back(std::meta::reflect_value(arg));
    }

    parse = extract<parser_type>(substitute(^^Unevaluated::template do_parse, parse_args));
  }
};

struct clap;

template <typename T>
  requires(std::is_aggregate_v<T> && !std::is_array_v<T>)
class Spec {
private:
  consteval auto get_arguments() {
    std::vector<Argument> fields;
    auto members = nonstatic_data_members_of(^^T);
    for (std::size_t idx = 0; idx < members.size(); ++idx) {
      if (!meta::has_annotation<annotations::Option>(members[idx])) {
        fields.emplace_back(idx, members[idx]);
      }
    }
    return fields;
  }

  consteval auto get_commands() {
    std::vector<Option<T>> fields;

    for (auto static_fnc : meta::static_member_functions_of(^^T)) {
      if (meta::has_annotation<annotations::Option>(static_fnc)) {
        fields.emplace_back(identifier_of(static_fnc), static_fnc);
      }
    }

    if constexpr (std::derived_from<T, clap>) {
      for (auto fnc_template :
           members_of(^^clap) | std::views::filter(std::meta::is_function_template)) {
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
    std::vector<Option<T>> fields;
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
  std::span<Option<T> const> commands;  // options that do not require a T
  std::span<Option<T> const> options;   // regular options

  consteval Spec()
      : arguments(std::meta::define_static_array(get_arguments()))
      , commands(std::meta::define_static_array(get_commands()))
      , options(std::meta::define_static_array(get_options())) {}
};

struct Program {
private:
  static std::string& get_name() {
    static std::string name;
    return name;
  }

public:
  static void set_name(std::string_view arg0) {
    if (arg0.empty()) {
      return;
    }
    auto path  = std::filesystem::path(arg0);
    get_name() = path.stem().string();
  }
  static std::string_view name() { return get_name(); }
};

struct clap {
  static constexpr annotations::Option option;
  static constexpr auto expect = annotations::expect;
  static constexpr auto value  = _expect_impl::Placeholder<0>{};
  static constexpr auto lazy   = _expect_impl::LazyProxy{};

  using shorthand   = annotations::Shorthand;
  using description = annotations::Description;

  template <typename T>
  [[= option]] [[noreturn]]
  static void help() {
    constexpr static Spec<std::remove_cvref_t<T>> spec{};
    auto program_name = Program::name();
    std::string arguments{};
    for (auto argument : spec.arguments) {
      if (argument.info.optional) {
        arguments += std::format("[{}] ", argument.info.name);
      } else {
        arguments += std::format("{} ", argument.info.name);
      }
    }
    std::println("usage: {} {}", program_name, arguments);
    std::println("Options:");

    auto print_option = [](auto opt) {
      std::string params;
      for (std::size_t idx = 0; idx < opt.argument_count; ++idx) {
        params += std::format("{}[{}] ", opt.arguments[idx].name, opt.arguments[idx].type);
      }

      std::println("--{} {}", opt.name, params);
      if (opt.description) {
        std::println("{:<10} {}", "", opt.description);
      }
    };

    for (auto command : spec.commands) {
      print_option(command);
    }

    for (auto opt : spec.options) {
      print_option(opt);
    }
    std::exit(0);
  }
};

template <typename T>
T parse_args(std::vector<std::string_view> args_in) {
  static constexpr Spec<T> spec{};
  auto parser          = ArgParser{args_in};
  auto& [args, cursor] = parser;

  Program::set_name(args[0]);
  cursor = 1;

  std::vector<typename Argument::Unevaluated> parsed_args{};
  for (auto Arg : spec.arguments) {
    typename Argument::Unevaluated argument{};
    if ((argument.*Arg.parse)(parser)) {
      parsed_args.push_back(argument);
    } else {
      break;
    }
  }

  std::vector<typename Option<T>::Unevaluated> parsed_opts{};
  auto [... Cmds] = [:meta::expand(spec.commands):];
  auto [... Opts] = [:meta::expand(spec.options):];

  while (parser.valid()) {
    bool found = false;
    typename Option<T>::Unevaluated option{};

    if (((option.*Cmds.parse)(parser) || ...)) {
      // run commands right away
      option(nullptr);
      continue;
    }

    found = ((option.*Opts.parse)(parser) || ...);
    if (!found) {
      parser.fail("Could not find option {}", parser.current());
    }
    parsed_opts.push_back(option);
  }

  ArgumentTuple<T> args_tuple;
  for (auto argument : parsed_args) {
    argument(&args_tuple);
  }

  // for (auto Arg : spec.arguments | std::views::drop(parsed_args.size())) {
  //   // default args
  //   Arg.check_default(parser, args_tuple);
  // }
  T object = default_construct<T>(args_tuple);

  // run options
  for (auto&& option : parsed_opts) {
    option(&object);
  }

  return object;
}

}  // namespace erl