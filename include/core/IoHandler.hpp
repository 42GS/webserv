/**
 * @file  IoHandler.hpp
 * @brief Interfaces between the event loop and what it serves.
 *        이벤트 루프와 루프가 다루는 대상 사이의 인터페이스.
 */
#ifndef CORE_IOHANDLER_HPP
#define CORE_IOHANDLER_HPP

#include <ctime>

/**
 * @brief Something registered with poll(): a listen socket, a client
 *        socket, or a CGI pipe.
 *        poll()에 등록하는 대상: listen 소켓, 클라이언트 소켓, CGI pipe.
 *
 * The event loop asks for the wanted events on every iteration, so
 * POLLOUT is requested only while there is data to send.
 * 이벤트 루프가 반복마다 원하는 이벤트를 묻기 때문에, 보낼 데이터가
 * 있을 때만 POLLOUT을 요청한다.
 */
class IoHandler {
 public:
  virtual ~IoHandler() {}

  /** @brief Descriptor to poll. / poll할 디스크립터. */
  virtual int fd() const = 0;

  /**
   * @brief poll() events wanted now (POLLIN, POLLOUT, or 0).
   *        지금 원하는 poll() 이벤트 (POLLIN, POLLOUT, 또는 0).
   */
  virtual short wantedEvents() const = 0;

  /**
   * @brief Handles the poll() result. Performs at most one read or write.
   *        poll() 결과를 처리한다. 읽기나 쓰기는 많아야 한 번 한다.
   * @param[in] revents Events from poll(). / poll()이 돌려준 이벤트.
   */
  virtual void onEvent(short revents) = 0;

  /**
   * @brief Called when onEvent() threw; the handler must end up closed.
   *        onEvent()가 예외를 던졌을 때 불린다. handler는 닫힌 상태가
   *        되어야 한다.
   */
  virtual void onFailure() = 0;
};

/**
 * @brief Registration with the event loop.
 *        이벤트 루프 등록.
 *
 * The registry never owns or deletes a handler.
 * registry는 handler를 소유하거나 지우지 않는다.
 */
class IoRegistry {
 public:
  virtual ~IoRegistry() {}

  /**
   * @brief Starts polling @p handler. / @p handler를 poll하기 시작한다.
   * @param[in] handler Handler; not owned. / handler. 소유하지 않는다.
   */
  virtual void watch(IoHandler* handler) = 0;

  /**
   * @brief Stops polling @p handler. / @p handler의 poll을 멈춘다.
   * @param[in] handler Handler. / handler.
   */
  virtual void unwatch(IoHandler* handler) = 0;
};

/**
 * @brief Called by the event loop at the end of every iteration.
 *        이벤트 루프가 반복이 끝날 때마다 부른다.
 */
class LoopObserver {
 public:
  virtual ~LoopObserver() {}

  /**
   * @brief Periodic work such as timeouts.
   *        시간 초과 검사 같은 주기적인 일.
   * @param[in] now Current time from the clock. / 시계의 현재 시각.
   */
  virtual void afterIteration(std::time_t now) = 0;

  /** @brief True to end run(). / run()을 끝내려면 true. */
  virtual bool shouldStop() const = 0;
};

#endif
