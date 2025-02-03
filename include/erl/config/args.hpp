#pragma once

namespace erl {
struct clap {
  static constexpr struct {
  } callback;
  struct shorthand {
    char const* data;
  };

  template <typename T>
  T parse(this T&& self, int argc, const char** argv){
    
    return self;
  }
};

template <typename T>
T parse_args(int argc, const char** argv){
  
}

}  // namespace erl