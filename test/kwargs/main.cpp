#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define KWARGS_PRINT 1
#include <kwargs.h>

TEST(Dummy, AlwaysPasses) {
  EXPECT_TRUE(true);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
