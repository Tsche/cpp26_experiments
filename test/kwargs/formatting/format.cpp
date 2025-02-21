#include <string_view>
#include <gtest/gtest.h>

#define KWARGS_FORMATTING 1
#include <kwargs.h>

using namespace std::string_view_literals;

TEST(Format, Formatting){
  EXPECT_EQ(erl::format("{foo} {bar}", make_args(foo=1, bar=2)), "1 2"sv);
  EXPECT_EQ(erl::format("{bar} {foo}", make_args(foo=1, bar=2)), "2 1"sv);
}