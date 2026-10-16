/**
 * @file  Signals.hpp
 * @brief Signal setup.
 *        시그널 설정.
 */
#ifndef CORE_SIGNALS_HPP
#define CORE_SIGNALS_HPP

/**
 * @brief Signal setup and the stop flag.
 *        시그널 설정과 종료 플래그.
 */
namespace signals {

/**
 * @brief Installs the signal actions.
 *        시그널 동작을 설치한다.
 *
 * SIGPIPE is ignored so that writing to a closed socket or pipe returns
 * an error instead of killing the server. SIGINT and SIGTERM only set a
 * flag; the event loop checks it and shuts down cleanly. SIGCHLD keeps
 * its default action so that waitpid() can collect children.
 * SIGPIPE는 무시한다. 닫힌 소켓이나 pipe에 쓰면 서버가 죽는 대신 오류가
 * 돌아온다. SIGINT와 SIGTERM은 플래그만 세우고, 이벤트 루프가 그것을
 * 보고 깨끗하게 끝난다. SIGCHLD는 기본 동작 그대로 두어 waitpid()로
 * 자식을 회수할 수 있게 한다.
 */
void install();

/**
 * @brief True once SIGINT or SIGTERM arrived.
 *        SIGINT나 SIGTERM이 오면 true.
 */
bool stopRequested();

}  // namespace signals

#endif
