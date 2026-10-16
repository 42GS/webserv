#include <poll.h>
#include <unistd.h>

#include <stdexcept>

#include "core/EventLoop.hpp"
#include "unit/TestFramework.hpp"

namespace {

const char* const kClockPath = "obj/.test_clock";

// Both ends of a pipe with one unread byte, so the read end is ready.
class ReadyPipe {
 public:
  ReadyPipe() {
    fds_[0] = -1;
    fds_[1] = -1;
    if (pipe(fds_) != 0 || write(fds_[1], "x", 1) != 1)
      throw std::runtime_error("cannot prepare a ready pipe");
  }
  ~ReadyPipe() {
    close(fds_[0]);
    close(fds_[1]);
  }
  int readEnd() const { return fds_[0]; }

 private:
  ReadyPipe(const ReadyPipe&);
  ReadyPipe& operator=(const ReadyPipe&);
  int fds_[2];
};

// Records what the loop did to it. Never reads, so it stays ready.
class RecordingHandler : public IoHandler {
 public:
  explicit RecordingHandler(int fd)
      : fd_(fd),
        events(0),
        failures(0),
        throwOnEvent(false),
        registry(0),
        toUnwatch(0) {}
  int fd() const { return fd_; }
  short wantedEvents() const { return POLLIN; }
  void onEvent(short /* revents */) {
    ++events;
    // Only once, so a later round shows whether the loop really dropped it.
    if (registry != 0 && toUnwatch != 0) {
      registry->unwatch(toUnwatch);
      toUnwatch = 0;
    }
    if (throwOnEvent) throw std::runtime_error("handler failed");
  }
  void onFailure() { ++failures; }

 private:
  int fd_;

 public:
  int events;
  int failures;
  bool throwOnEvent;
  IoRegistry* registry;
  IoHandler* toUnwatch;
};

class StopAfterRounds : public LoopObserver {
 public:
  explicit StopAfterRounds(int rounds) : remaining_(rounds) {}
  void afterIteration(std::time_t /* now */) { --remaining_; }
  bool shouldStop() const { return remaining_ <= 0; }

 private:
  int remaining_;
};

}  // namespace

TEST_CASE(loopDeliversReadyEvents) {
  ReadyPipe ready;
  RecordingHandler handler(ready.readEnd());
  Clock clock(kClockPath);
  EventLoop loop(clock);
  loop.watch(&handler);
  StopAfterRounds observer(2);
  loop.run(observer);
  CHECK_EQ(2, handler.events);
  CHECK_EQ(0, handler.failures);
}

TEST_CASE(unwatchDuringDispatchSkipsTheRestOfTheRound) {
  ReadyPipe first;
  ReadyPipe second;
  RecordingHandler closer(first.readEnd());
  RecordingHandler closed(second.readEnd());
  Clock clock(kClockPath);
  EventLoop loop(clock);
  loop.watch(&closer);
  loop.watch(&closed);
  closer.registry = &loop;
  closer.toUnwatch = &closed;
  StopAfterRounds observer(2);
  loop.run(observer);
  CHECK_EQ(2, closer.events);
  CHECK_EQ(0, closed.events);
}

TEST_CASE(exceptionFromHandlerCallsOnFailureOnly) {
  ReadyPipe first;
  ReadyPipe second;
  RecordingHandler failing(first.readEnd());
  RecordingHandler healthy(second.readEnd());
  failing.throwOnEvent = true;
  Clock clock(kClockPath);
  EventLoop loop(clock);
  loop.watch(&failing);
  loop.watch(&healthy);
  StopAfterRounds observer(1);
  loop.run(observer);
  CHECK_EQ(1, failing.failures);
  CHECK_EQ(1, healthy.events);
  CHECK_EQ(0, healthy.failures);
}
