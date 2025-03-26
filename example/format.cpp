#include <erl/print.hpp>

struct Foo {
  int bar;
};

struct Point {
  int x;
  int y;
  Foo zoinks;
};

int main() {
  auto point  = Point{42, 69, {3}};
  auto point2 = Point{777, 555};
  erl::println("{0.zoinks.bar}", point);
  erl::println("{} {}", point.x, point.y);
  erl::println("{0} {1}", point.x, point.y);

  erl::println("{1} foo {0.x} {0.y}", point, 2);

  erl::println("{1.x} {{2}} {3.y} {1.y} {2} {0}", point.x, point, point.y, point2);
}