#include "core/Signals.hpp"

#include <csignal>

namespace {

// The only global state in the server: a signal handler can reach nothing
// else (design 10).
volatile std::sig_atomic_t g_stopRequested = 0;

void requestStop(int) { g_stopRequested = 1; }

}  // namespace

namespace signals {

// std::signal() fails only for an invalid or uncatchable signal number, so
// its result is not checked for these three.
void install() {
  std::signal(SIGPIPE, SIG_IGN);
  std::signal(SIGINT, requestStop);
  std::signal(SIGTERM, requestStop);
}

bool stopRequested() { return g_stopRequested != 0; }

}  // namespace signals
