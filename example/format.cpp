#include <erl/print>

struct Foo {
  int x;
  std::vector<int> zoinks;
};

struct Point {
  int x;
  int y;
  Foo zoinks;
};

struct Test {
  std::vector<Foo> member;
  int x;
};

int main() {
  auto point  = Point{42, 69, {3}};
  auto point2 = Point{777, 555};

  erl::println("{0.zoinks.x}", point);
  erl::println("{} {}", point.x, point.y);
  erl::println("{0} {1}", point.x, point.y);
  erl::println("{x} {y}", point);

  erl::println("{1.0} {{2}} {3.y} {1.y} {2} {0}", point.x, point, point.y, point2);

  std::tuple<int, char> some_tuple = {42, 'c'};
  erl::println("{0.0} {0.1}", some_tuple);

  std::vector<int> some_vector = {0, 7, 42};
  erl::println("{0.0} {0.1} {0.2}", some_vector);

  char array[]{"foo"};
  erl::println("{0.0} {0.1} {0.2}", array);

  // auto test = Test{.member={Foo{1, {2, 3}}}};
  auto test = Test{.member={Foo{42, {69, 2}}}};
  erl::println("{member.0.zoinks.1}", test);
}