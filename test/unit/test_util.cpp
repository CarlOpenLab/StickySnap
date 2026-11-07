// util::Clamp 边界
#include <type_traits>

#include "test_util.h"

#include "config.h"
#include "util.h"

TEST_CASE(util_clamp_int_bounds) {
  EXPECT_EQ(::util::Clamp(5, 0, 10), 5);
  EXPECT_EQ(::util::Clamp(0, 0, 10), 0);
  EXPECT_EQ(::util::Clamp(10, 0, 10), 10);
  EXPECT_EQ(::util::Clamp(-1, 0, 10), 0);
  EXPECT_EQ(::util::Clamp(11, 0, 10), 10);
  EXPECT_EQ(::util::Clamp(-50, -10, -5), -10);
}

TEST_CASE(util_clamp_double_bounds) {
  EXPECT_EQ(::util::Clamp(9.0, ::config::kZoomMin, ::config::kZoomMax), 8.0);
  EXPECT_EQ(::util::Clamp(0.05, ::config::kZoomMin, ::config::kZoomMax), 0.1);
  EXPECT_EQ(::util::Clamp(1.0, ::config::kZoomMin, ::config::kZoomMax), 1.0);
  EXPECT_EQ(::util::Clamp(::config::kZoomMax, ::config::kZoomMin, ::config::kZoomMax),
            ::config::kZoomMax);
}

TEST_CASE(util_clamp_result_type_follows_value) {
  // 以 v 的类型为准：v 是 double 时结果仍是 double
  EXPECT_TRUE((std::is_same<decltype(::util::Clamp(1.0, 0, 2)), double>::value));
  EXPECT_TRUE((std::is_same<decltype(::util::Clamp(1, 0, 2)), int>::value));
}
