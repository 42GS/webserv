#ifndef TESTS_UNIT_EVENTLOOPDOUBLES_HPP
#define TESTS_UNIT_EVENTLOOPDOUBLES_HPP

#include <poll.h>
#include <unistd.h>

#include <ctime>
#include <stdexcept>

#include "core/IoHandler.hpp"

// Stand-ins for the objects EventLoop works with (event_loop_test.cpp).

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

#endif
