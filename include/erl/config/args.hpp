#pragma once

#include <experimental/meta>
#include <cstdio>
#include <print>
#include <ranges>
#include <erl/util/meta.hpp>
#include <utility>
#include <filesystem>

#include "default_construct.hpp"
#include "annotations.hpp"
#include "erl/config/expect.hpp"

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

template <typename T, std::size_t Idx>
void check_default_initializable(ArgumentTuple<T> const& arguments) {
  constexpr static auto member = nonstatic_data_members_of(^^T)[Idx];
  if constexpr (!has_default_member_initializer(member)) {
    throw std::out_of_range(std::format("Missing required argument: {}", identifier_of(member)));
  }
}

// template <typename T>
// struct ArgParser;

template <typename T>
struct Argument {
  struct Unevaluated {
    using handler_type = void (Unevaluated::*)(ArgumentTuple<T>&) const;

    std::string_view argument;
    handler_type handle;
    void operator()(ArgumentTuple<T>& args) const { (this->*handle)(args); }

    template <std::size_t Idx>
    void do_handle(ArgumentTuple<T>& arguments) const {
      using type          = [:type_of(nonstatic_data_members_of(^^T)[Idx]):];
      get<Idx>(arguments) = parse_value<type>(argument);
    }

    template <std::size_t Idx>
    bool do_parse(std::span<std::string_view> args, std::size_t& cursor) {
      static constexpr auto current_handler =
          meta::instantiate<handler_type>(^^do_handle, std::meta::reflect_value(Idx));

      if (cursor >= args.size()) {
        return false;
      }

      if (args[cursor][0] == '-') {
        // TODO check if int
        return false;
      }

      argument = args[cursor];
      handle   = current_handler;
      return true;
    }
  };

  using parser_type  = bool (Unevaluated::*)(std::span<std::string_view>, std::size_t&);
  using default_type = void (*)(ArgumentTuple<T> const&);
  parser_type parse;
  default_type check_default;
  bool optional = false;
  char const* type;
  char const* name;

  consteval Argument(std::size_t idx, std::meta::info reflection)
      : parse(meta::instantiate<parser_type>(^^Unevaluated::template do_parse,
                                             std::meta::reflect_value(idx)))
      , check_default(meta::instantiate<default_type>(^^check_default_initializable,
                                                      ^^T,
                                                      std::meta::reflect_value(idx)))
      , optional(has_default_member_initializer(reflection))
      , type(std::meta::define_static_string(display_string_of(type_of(reflection))))
      , name(std::meta::define_static_string(identifier_of(reflection))) {}
};

template <typename T>
struct Option {
  struct Unevaluated {
    using handler_type = void (Unevaluated::*)(T*) const;
    std::vector<std::string_view> arguments;
    handler_type handle;
    void operator()(T* object) const { (this->*handle)(object); }

    template <std::meta::info R, typename... Ts>
    void do_handle(T* obj) const {
      if constexpr (parent_of(R) == ^^T) {
        if constexpr (is_object_type(type_of(R))) {
          static_assert(sizeof...(Ts) == 1);
          obj->[:R:] = (parse_value<Ts>(arguments[0]), ...);
        } else {
          auto [... idx] = std::index_sequence_for<Ts...>();
          obj->[:R:](parse_value<Ts>(arguments[idx])...);
        }
      } else {
        auto [... idx] = std::index_sequence_for<Ts...>();
        [:R:](parse_value<Ts>(arguments[idx])...);
      }
    }

    static consteval handler_type make_handler(std::meta::info R) {
      std::vector args = {reflect_value(R)};
      if (is_object_type(type_of(R))) {
        args.push_back(type_of(R));
      } else {
        for (auto param : parameters_of(R)) {
          args.push_back(type_of(param));
        }
      }
      return extract<handler_type>(substitute(^^do_handle, args));
    }

    template <util::fixed_string name, std::meta::info R>
    bool do_parse(std::span<std::string_view> args, std::size_t& cursor) {
      static constexpr auto current_handler = make_handler(R);

      // TODO flags
      constexpr static auto parameter_count =
          is_object_type(type_of(R)) ? 1 : parameters_of(R).size();

      auto arg_name = args[cursor];
      arg_name.remove_prefix(2);
      if (arg_name != name) {
        return false;
      }

      for (int idx = 0; idx < parameter_count; ++idx) {
        if (cursor >= args.size()) {
          // if (!std::meta::has_default_argument(param)){
          //   // TODO error
          //   return false;
          // }
          return false;
        }
        arguments.push_back(args[++cursor]);
      }

      handle = current_handler;
      return true;
    }
  };

  using parser_type = bool (Unevaluated::*)(std::span<std::string_view>, std::size_t&);

  parser_type parse;
  char const* name;
  char const* description = nullptr;

  std::size_t argument_size;
  char const* const* argument_types;
  char const* const* argument_names;

  consteval Option(std::string_view name, std::meta::info reflection)
      : parse(meta::instantiate<parser_type>(^^Unevaluated::template do_parse,
                                             meta::promote(name),
                                             reflect_value(reflection)))
      , name(std::meta::define_static_string(name)) {
    if (auto desc = annotation_of_type<annotations::Description>(reflection); desc) {
      description = desc->data;
    }

    std::vector<char const*> types;
    std::vector<char const*> names;
    if (is_object_type(type_of(reflection))) {
      types.push_back(std::meta::define_static_string(display_string_of(type_of(reflection))));
      names.push_back(std::meta::define_static_string(identifier_of(reflection)));
    } else {
      for (auto param : parameters_of(reflection)) {
        types.push_back(std::meta::define_static_string(display_string_of(type_of(param))));
        names.push_back(std::meta::define_static_string(identifier_of(param)));
      }
    }
    argument_size  = names.size();
    argument_types = std::meta::define_static_array(types).data();
    argument_names = std::meta::define_static_array(names).data();
  }
};

struct clap;

template <typename T>
  requires(std::is_aggregate_v<T> && !std::is_array_v<T>)
class Spec {
private:
  consteval auto get_arguments() {
    std::vector<Argument<T>> fields;
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
  std::span<Argument<T> const> arguments;  // arguments required or usable to construct T
  std::span<Option<T> const> commands;     // options that do not require a T
  std::span<Option<T> const> options;      // regular options

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
  static constexpr auto value = _expect_impl::Placeholder<0>{};
  static constexpr auto lazy = _expect_impl::LazyProxy{};

  using shorthand   = annotations::Shorthand;
  using description = annotations::Description;

  template <typename T>
  [[= option]] [[noreturn]]
  static void help() {
    constexpr static Spec<std::remove_cvref_t<T>> spec{};
    auto program_name = Program::name();
    std::string arguments{};
    for (auto argument : spec.arguments) {
      if (argument.optional) {
        arguments += std::format("[{}: {}] ", argument.name, argument.type);
      } else {
        arguments += std::format("{}: {} ", argument.name, argument.type);
      }
    }
    std::println("usage: {} {}", program_name, arguments);
    std::println("Options:");

    auto print_option = [](auto opt) {
      std::string params;
      for (std::size_t idx = 0; idx < opt.argument_size; ++idx) {
        params += std::format("{}[{}] ", opt.argument_names[idx], opt.argument_types[idx]);
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
T parse_args(std::vector<std::string_view> args) {
  static constexpr Spec<T> spec{};

  Program::set_name(args[0]);
  std::size_t cursor = 1;

  std::vector<typename Argument<T>::Unevaluated> parsed_args{};
  for (auto Arg : spec.arguments) {
    typename Argument<T>::Unevaluated argument{};
    if ((argument.*Arg.parse)(args, cursor)) {
      parsed_args.push_back(argument);
      ++cursor;
    } else {
      break;
    }
  }

  std::vector<typename Option<T>::Unevaluated> parsed_opts{};
  auto [... Cmds] = [:meta::expand(spec.commands):];
  auto [... Opts] = [:meta::expand(spec.options):];

  for (; cursor < args.size(); ++cursor) {
    bool found = false;
    typename Option<T>::Unevaluated option{};

    if (((option.*Cmds.parse)(args, cursor) || ...)) {
      // run commands right away
      option(nullptr);
      continue;
    }

    found = ((option.*Opts.parse)(args, cursor) || ...);
    if (!found) {
      std::println("Could not find option {}", args[cursor]);
      std::exit(1);
    }
    parsed_opts.push_back(option);
  }

  ArgumentTuple<T> args_tuple;
  for (auto argument : parsed_args) {
    argument(args_tuple);
  }

  for (auto Arg : spec.arguments | std::views::drop(parsed_args.size())) {
    // default args
    Arg.check_default(args_tuple);
  }
  T object = default_construct<T>(args_tuple);

  // run options
  for (auto&& option : parsed_opts) {
    option(&object);
  }

  return object;
}

}  // namespace erl