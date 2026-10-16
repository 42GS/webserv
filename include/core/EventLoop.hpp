/**
 * @file  EventLoop.hpp
 * @brief The single poll() loop of the server.
 *        서버의 유일한 poll() 루프.
 */
#ifndef CORE_EVENTLOOP_HPP
#define CORE_EVENTLOOP_HPP

#include <set>
#include <vector>

#include "core/Clock.hpp"
#include "core/IoHandler.hpp"

/**
 * @brief The single poll() loop for every socket and pipe.
 *        모든 소켓과 pipe를 다루는 하나의 poll() 루프.
 *
 * It owns nothing: owners create handlers, watch() them, and delete them
 * after unwatch().
 * 아무것도 소유하지 않는다. 소유자가 handler를 만들어 watch()하고,
 * unwatch()한 뒤에 지운다.
 */
class EventLoop : public IoRegistry {
 public:
  /**
   * @brief Uses @p clock for the time passed to the observer.
   *        observer에 넘길 시각을 @p clock에서 얻는다.
   * @param[in,out] clock Refreshed once per iteration; kept by reference.
   *                      반복마다 한 번 갱신한다. 참조로 보관한다.
   */
  explicit EventLoop(Clock& clock);

  /**
   * @brief Adds a handler to the poll set.
   *        handler를 poll 대상에 넣는다.
   * @param[in] handler Handler; not owned. / handler. 소유하지 않는다.
   */
  void watch(IoHandler* handler);

  /**
   * @brief Removes a handler from the poll set.
   *        handler를 poll 대상에서 뺀다.
   *
   * During dispatch the handler is only marked; it is dropped from the
   * list after the current round, and gets no further events in this
   * round. The caller may delete it once unwatch() returns.
   * 이벤트를 나눠 주는 중이면 표시만 해 두고 이번 회차가 끝난 뒤에
   * 목록에서 뺀다. 이번 회차의 남은 이벤트는 받지 않는다. unwatch()가
   * 돌아오면 호출자가 지워도 된다.
   *
   * @param[in] handler Handler. / handler.
   */
  void unwatch(IoHandler* handler);

  /**
   * @brief Runs until observer.shouldStop().
   *        observer.shouldStop()이 참이 될 때까지 돈다.
   *
   * The clock is refreshed once per iteration and its value is passed to
   * observer.afterIteration().
   * 반복마다 시계를 한 번 갱신하고 그 값을 observer.afterIteration()에
   * 넘긴다.
   *
   * @param[in,out] observer Called after every iteration.
   *                         반복이 끝날 때마다 부른다.
   */
  void run(LoopObserver& observer);

 private:
  /**
   * @brief One poll() call and the dispatch of its events.
   *        poll() 한 번과 그 이벤트 분배.
   */
  void pollOnce();

  /**
   * @brief Calls onEvent() of every polled handler that has events.
   *        이벤트가 있는 handler마다 onEvent()를 부른다.
   *
   * A handler that throws gets onFailure() instead of stopping the loop.
   * 예외를 던진 handler는 루프를 멈추는 대신 onFailure()를 받는다.
   *
   * @param[in] polled  Handlers in poll() order. / poll() 순서의 handler.
   * @param[in] revents Their events. / 각 handler의 이벤트.
   */
  void dispatch(const std::vector<IoHandler*>& polled,
                const std::vector<short>& revents);

  /**
   * @brief Removes the marked handlers.
   *        표시해 둔 handler를 목록에서 뺀다.
   */
  void dropUnwatched();

  /** Time source. / 시각을 얻는 곳. */
  Clock& clock_;
  /** Watched handlers. / 감시 중인 handler. */
  std::vector<IoHandler*> handlers_;
  /** Marked for removal. / 뺄 것으로 표시한 handler. */
  std::set<IoHandler*> unwatched_;
  /** Inside dispatch(). / dispatch() 실행 중. */
  bool dispatching_;
};

#endif
