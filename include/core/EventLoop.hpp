#ifndef CORE_EVENTLOOP_HPP
#define CORE_EVENTLOOP_HPP

#include <set>
#include <vector>

#include "core/Clock.hpp"
#include "core/IoHandler.hpp"

// The single poll() loop for every socket and pipe. It owns nothing:
// owners create handlers, watch() them, and delete them after unwatch().
class EventLoop : public IoRegistry {
 public:
  explicit EventLoop(Clock& clock);

  void watch(IoHandler* handler);
  // During dispatch the handler is only marked; it is dropped from the
  // list after the current round, and gets no further events in this round.
  void unwatch(IoHandler* handler);

  // Runs until observer.shouldStop(). The clock is refreshed once per
  // iteration and its value is passed to observer.afterIteration().
  void run(LoopObserver& observer);

 private:
  void pollOnce();
  void dispatch(const std::vector<IoHandler*>& polled,
                const std::vector<short>& revents);
  void dropUnwatched();

  Clock& clock_;
  std::vector<IoHandler*> handlers_;
  std::set<IoHandler*> unwatched_;
  bool dispatching_;
};

#endif
