/**
 * @file  Signals.cpp
 * @brief Signal handlers and the stop flag.
 *        시그널 handler와 종료 플래그.
 */
#include "core/Signals.hpp"

#include <csignal>

namespace {

/**
 * @brief Set by SIGINT and SIGTERM.
 *        SIGINT와 SIGTERM이 세우는 플래그.
 *
 * The only global state in the server: a signal handler can reach nothing
 * else (design 10).
 * 서버의 유일한 전역 상태다. 시그널 handler는 다른 것에 닿을 수 없다
 * (설계 10절).
 */
volatile std::sig_atomic_t g_stopRequested = 0;

/** @brief Handler of SIGINT and SIGTERM. / SIGINT와 SIGTERM의 handler. */
void requestStop(int) { g_stopRequested = 1; }

}  // namespace

namespace signals {

// std::signal() fails only for an invalid or uncatchable signal number, so
// its result is not checked for these three.
// std::signal()은 잘못됐거나 잡을 수 없는 시그널 번호일 때만 실패한다.
// 그래서 이 세 시그널에서는 결과를 보지 않는다.
void install() {
  std::signal(SIGPIPE, SIG_IGN);
  std::signal(SIGINT, requestStop);
  std::signal(SIGTERM, requestStop);
}

bool stopRequested() { return g_stopRequested != 0; }

}  // namespace signals
