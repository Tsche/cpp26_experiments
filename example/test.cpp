#include <print>
#include <experimental/meta>

template <typename...>
struct Foo{};

int main() {
    std::println("{}", display_string_of(^^Foo<>));
}