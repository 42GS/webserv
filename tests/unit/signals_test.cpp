/**
 * @file  signals_test.cpp
 * @brief Unit tests of the signal setup.
 *        시그널 설정의 단위 테스트.
 *
 * Test IDs (N-xx) refer to the test plan in the wiki
 * (analysis/test-plan).
 * 테스트 ID(N-xx)는 위키의 테스트 계획(analysis/test-plan)을 가리킨다.
 */
#include "core/Signals.hpp"

#include <csignal>

#include "unit/TestFramework.hpp"

namespace {

/**
 * @brief The disposition in effect.
 *        현재 적용 중인 처리 방식.
 *
 * Read by setting the default and putting the old one back (std::signal
 * returns the previous handler).
 * 기본값을 설정했다가 이전 것을 되돌려 놓으며 읽는다 (std::signal은 이전
 * handler를 돌려준다).
 *
 * @param[in] signal Signal number. / 시그널 번호.
 * @return SIG_DFL, SIG_IGN or a handler. / SIG_DFL, SIG_IGN 또는 handler.
 */
void (*dispositionOf(int signal))(int) {
  void (*previous)(int) = std::signal(signal, SIG_DFL);
  std::signal(signal, previous);
  return previous;
}

}  // namespace

// SIGPIPE ends the process by default; the server ignores it. Integration
// tests cannot provoke it reliably: the server sees POLLERR/POLLHUP of a
// reset peer before a second send().
// SIGPIPE는 기본으로 프로세스를 끝내고, 서버는 이를 무시한다. 통합 테스트는
// 이를 확실히 일으킬 수 없다. 서버가 두 번째 send() 전에 끊긴 상대의
// POLLERR/POLLHUP을 먼저 본다.
TEST_CASE(n07_sigpipeIsIgnoredAndSigchldLeftAlone) {
  signals::install();
  CHECK(dispositionOf(SIGPIPE) == SIG_IGN);
  CHECK(dispositionOf(SIGCHLD) == SIG_DFL);
  CHECK(dispositionOf(SIGINT) != SIG_DFL);
  CHECK(dispositionOf(SIGTERM) != SIG_DFL);
  CHECK(!signals::stopRequested());
}
