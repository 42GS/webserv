#include "core/Clock.hpp"

#include "unit/TestConstants.hpp"
#include "unit/TestFramework.hpp"

TEST_CASE(clockReadsTimeFromFile) {
  Clock clock(test_paths::kClockFile);
  CHECK(clock.refresh());
  const std::time_t first = clock.now();
  CHECK(first > 0);
  CHECK(clock.refresh());
  CHECK(clock.now() >= first);
}

TEST_CASE(clockReportsFailureAndKeepsOldValue) {
  Clock clock(clock_test::kUnwritableClockPath);
  CHECK(!clock.refresh());
  CHECK_EQ(static_cast<std::time_t>(0), clock.now());
}
