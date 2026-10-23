/**
 * @file  EventLoopDoubles.hpp
 * @brief Stand-ins for the objects EventLoop works with
 *        (event_loop_test.cpp).
 *        EventLoop가 다루는 객체의 대역 (event_loop_test.cpp).
 */
#ifndef TESTS_UNIT_EVENTLOOPDOUBLES_HPP
#define TESTS_UNIT_EVENTLOOPDOUBLES_HPP

#include <poll.h>
#include <unistd.h>

#include <ctime>
#include <stdexcept>

#include "core/IoHandler.hpp"

/**
 * @brief Both ends of a pipe with one unread byte, so the read end is
 *        ready.
 *        읽지 않은 바이트 하나가 든 pipe의 양 끝. 그래서 읽는 쪽이 준비돼
 *        있다.
 */
class ReadyPipe {
 public:
  /**
   * @brief Opens the pipe and writes the byte.
   *        pipe를 열고 바이트를 쓴다.
   * @throws std::runtime_error If either fails. / 둘 중 하나가 실패하면.
   */
  ReadyPipe() {
    fds_[0] = -1;
    fds_[1] = -1;
    if (pipe(fds_) != 0 || write(fds_[1], "x", 1) != 1)
      throw std::runtime_error("cannot prepare a ready pipe");
  }
  /** @brief Closes both ends. / 양 끝을 닫는다. */
  ~ReadyPipe() {
    close(fds_[0]);
    close(fds_[1]);
  }

  /** @brief The ready end. / 준비된 쪽. */
  int readEnd() const { return fds_[0]; }

 private:
  /// @cond
  // Not copyable: declared and never defined (C++98 has no "= delete").
  // 복사하지 않는다: 선언만 하고 정의하지 않는다 (C++98에는 "= delete"가 없다).
  ReadyPipe(const ReadyPipe&);
  ReadyPipe& operator=(const ReadyPipe&);
  /// @endcond

  /** Read end, then write end. / 읽는 쪽, 그다음 쓰는 쪽. */
  int fds_[2];
};

/**
 * @brief Records what the loop did to it. Never reads, so it stays ready.
 *        루프가 자신에게 한 일을 기록한다. 읽지 않으므로 계속 준비 상태다.
 */
class RecordingHandler : public IoHandler {
 public:
  /**
   * @brief A handler of fd that records only.
   *        기록만 하는 fd의 handler.
   * @param[in] fd Descriptor to report. / 알릴 디스크립터.
   */
  explicit RecordingHandler(int fd)
      : fd_(fd),
        events(0),
        failures(0),
        throwOnEvent(false),
        registry(0),
        toUnwatch(0) {}

  /** @brief The descriptor. / 디스크립터. */
  int fd() const { return fd_; }
  /** @brief Always POLLIN. / 항상 POLLIN. */
  short wantedEvents() const { return POLLIN; }

  /**
   * @brief Counts the event, then unwatches or throws as set up.
   *        이벤트를 세고, 설정한 대로 감시를 끊거나 예외를 던진다.
   * @throws std::runtime_error If throwOnEvent is set. / throwOnEvent가
   *         켜져 있으면.
   */
  void onEvent(short /* revents */) {
    ++events;
    // Only once, so a later round shows whether the loop really dropped it.
    // 한 번만 한다. 그래야 다음 라운드에서 루프가 정말 뺐는지 보인다.
    if (registry != 0 && toUnwatch != 0) {
      registry->unwatch(toUnwatch);
      toUnwatch = 0;
    }
    if (throwOnEvent) throw std::runtime_error("handler failed");
  }
  /** @brief Counts the failure. / 실패를 센다. */
  void onFailure() { ++failures; }

 private:
  /** Descriptor. / 디스크립터. */
  int fd_;

 public:
  /** onEvent() calls. / onEvent() 호출 수. */
  int events;
  /** onFailure() calls. / onFailure() 호출 수. */
  int failures;
  /** Throw from onEvent(). / onEvent()에서 예외를 던진다. */
  bool throwOnEvent;
  /** Registry for toUnwatch. / toUnwatch에 쓸 registry. */
  IoRegistry* registry;
  /** Unwatched on the next event. / 다음 이벤트에서 감시를 끊는다. */
  IoHandler* toUnwatch;
};

/**
 * @brief Stops the loop after a number of rounds.
 *        정한 라운드 수가 지나면 루프를 멈춘다.
 */
class StopAfterRounds : public LoopObserver {
 public:
  /**
   * @brief Counts down from rounds.
   *        rounds부터 거꾸로 센다.
   * @param[in] rounds Rounds to run. / 돌 라운드 수.
   */
  explicit StopAfterRounds(int rounds) : remaining_(rounds) {}

  /** @brief Counts one round. / 라운드 하나를 센다. */
  void afterIteration(std::time_t /* now */) { --remaining_; }
  /** @brief True once the rounds are used up. / 라운드를 다 쓰면 true. */
  bool shouldStop() const { return remaining_ <= 0; }

 private:
  /** Rounds left. / 남은 라운드 수. */
  int remaining_;
};

#endif
