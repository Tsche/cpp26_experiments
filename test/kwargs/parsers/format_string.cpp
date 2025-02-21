#include <source_location>
#include <string_view>
#include <vector>
#include <gtest/gtest.h>

#define KWARGS_FORMATTING 1
#include <kwargs.h>

void check(std::string_view str, std::vector<std::string_view> const& names, std::string_view expected, std::source_location const& loc = std::source_location::current()){
  auto parser = erl::formatting::FmtParser{str};
  auto result = parser.transform(names);

  auto source_location = std::format("{}:{}", loc.file_name(), loc.line());
  EXPECT_EQ(result, expected) << source_location;
}

TEST(FormatString, Matching) {
  std::vector<std::string_view> names = {"x", "y"};
  check("", names, "");
  check("{x} {y}", names, "{0} {1}");
  check("{y} {x}", names, "{1} {0}");
  check("{x} {x}", names, "{0} {0}");
  check("{y} {y}", names, "{1} {1}");
  check("foo{y}foo{x}foo", names, "foo{1}foo{0}foo");
  check("{{x}}", names, "{{x}}");
  check("{{x{x}}}", names, "{{x{x}}}");
  check("{x}{{x{x}}}{x}", names, "{0}{{x{x}}}{0}");
}

TEST(FormatString, Mismatch) {
  check("{foo}", {}, "{0}");
  check("{foo}", {"x"}, "{1}");
  check("{x}{foo}", {}, "{0}{0}");
  check("{x}{foo}", {"x"}, "{0}{1}");
  check("{x}{foo}", {"foo"}, "{1}{0}");
}