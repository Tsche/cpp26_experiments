#include <gtest/gtest.h>
#include <kwargs.h>

TEST(Util, Utos) {
  ASSERT_EQ(erl::util::utos(1),   "1");
  ASSERT_EQ(erl::util::utos(10),  "10");
  ASSERT_EQ(erl::util::utos(111), "111");
}
