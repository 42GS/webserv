#include "core/Clock.hpp"

#include "unit/TestFramework.hpp"

namespace {

// Relative to the repository root, where "make test" runs the tests.
const char* const kWritableClockPath = "obj/.test_clock";
const char* const kUnwritableClockPath = "/nonexistent/dir/clock";

}  // namespace

TEST_CASE(clockReadsTimeFromFile) {
  Clock clock(kWritableClockPath);
  CHECK(clock.refresh());
  const std::time_t first = clock.now();
  CHECK(first > 0);
  CHECK(clock.refresh());
  CHECK(clock.now() >= first);
}

TEST_CASE(clockReportsFailureAndKeepsOldValue) {
  Clock clock(kUnwritableClockPath);
  CHECK(!clock.refresh());
  CHECK_EQ(static_cast<std::time_t>(0), clock.now());
}
