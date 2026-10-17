#include "core/Signals.hpp"

#include <csignal>

#include "unit/TestFramework.hpp"

// Test IDs (N-xx) refer to the test plan in the wiki (analysis/test-plan).

namespace {

// The disposition in effect, read by setting the default and putting the
// old one back (std::signal returns the previous handler).
void (*dispositionOf(int signal))(int) {
  void (*previous)(int) = std::signal(signal, SIG_DFL);
  std::signal(signal, previous);
  return previous;
}

}  // namespace

// SIGPIPE ends the process by default; the server ignores it. Integration
// tests cannot provoke it reliably: the server sees POLLERR/POLLHUP of a
// reset peer before a second send().
TEST_CASE(n07_sigpipeIsIgnoredAndSigchldLeftAlone) {
  signals::install();
  CHECK(dispositionOf(SIGPIPE) == SIG_IGN);
  CHECK(dispositionOf(SIGCHLD) == SIG_DFL);
  CHECK(dispositionOf(SIGINT) != SIG_DFL);
  CHECK(dispositionOf(SIGTERM) != SIG_DFL);
  CHECK(!signals::stopRequested());
}
