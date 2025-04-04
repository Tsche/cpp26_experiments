#include <gtest/gtest.h>
#include <kwargs.h>


template <typename T>
requires requires(T const& kwargs) {
    { kwargs.x } -> std::convertible_to<int>;
}
int member_access(erl::kwargs_t<T> const& kwargs) {
  return kwargs.x;
}

template <typename T>
int get_by_name(erl::kwargs_t<T> const& kwargs) {
  return get<"x">(kwargs);
}

template <typename T>
int get_by_name_default(erl::kwargs_t<T> const& kwargs) {
  return get_or<"x">(kwargs, 42);
}

template <typename T>
int get_by_idx(erl::kwargs_t<T> const& kwargs) {
  return get<0>(kwargs);
}

template <typename T>
int get_by_idx_default(erl::kwargs_t<T> const& kwargs) {
  return get_or<0>(kwargs, 42);
}

TEST(KwArgs, Empty) {
  auto args = make_args();
  EXPECT_EQ(std::tuple_size_v<decltype(args)>, 0);
}

TEST(KwArgs, Simple) {
  EXPECT_EQ(member_access(make_args(x=10)), 10);
  EXPECT_EQ(get_by_idx(make_args(x=10)), 10);
  EXPECT_EQ(get_by_name(make_args(x=10)), 10);
}

TEST(KwArgs, Default) {
  EXPECT_EQ(get_by_name_default(make_args(x=10)), 10);
  EXPECT_EQ(get_by_idx_default(make_args(x=10)), 10);
  EXPECT_EQ(get_by_name_default(make_args()), 42);
  EXPECT_EQ(get_by_idx_default(make_args()), 42);
}