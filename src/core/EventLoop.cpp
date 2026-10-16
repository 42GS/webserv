#include "core/EventLoop.hpp"

#include <poll.h>

#include <algorithm>
#include <exception>

namespace {

const int kPollTimeoutMs = 1000;

}  // namespace

EventLoop::EventLoop(Clock& clock) : clock_(clock), dispatching_(false) {}

void EventLoop::watch(IoHandler* handler) { handlers_.push_back(handler); }

void EventLoop::unwatch(IoHandler* handler) {
  unwatched_.insert(handler);
  if (!dispatching_) dropUnwatched();
}

void EventLoop::run(LoopObserver& observer) {
  while (!observer.shouldStop()) {
    pollOnce();
    // On failure the old time is kept; timeouts are only delayed.
    clock_.refresh();
    observer.afterIteration(clock_.now());
  }
}

void EventLoop::pollOnce() {
  std::vector<IoHandler*> polled(handlers_);
  std::vector<pollfd> fds(polled.size());
  for (std::size_t i = 0; i < polled.size(); ++i) {
    fds[i].fd = polled[i]->fd();
    fds[i].events = polled[i]->wantedEvents();
    fds[i].revents = 0;
  }
  // -1 means interrupted by a signal or a failure; errno is not consulted.
  // The loop goes on and the observer decides whether to stop.
  const int ready = poll(fds.empty() ? 0 : &fds[0],
                         static_cast<nfds_t>(fds.size()), kPollTimeoutMs);
  if (ready <= 0) return;
  std::vector<short> revents(fds.size());
  for (std::size_t i = 0; i < fds.size(); ++i) revents[i] = fds[i].revents;
  dispatch(polled, revents);
}

void EventLoop::dispatch(const std::vector<IoHandler*>& polled,
                         const std::vector<short>& revents) {
  dispatching_ = true;
  for (std::size_t i = 0; i < polled.size(); ++i) {
    if (revents[i] == 0 || unwatched_.count(polled[i]) != 0) continue;
    try {
      polled[i]->onEvent(revents[i]);
    } catch (const std::exception&) {
      polled[i]->onFailure();
    }
  }
  dispatching_ = false;
  dropUnwatched();
}

void EventLoop::dropUnwatched() {
  for (std::set<IoHandler*>::const_iterator it = unwatched_.begin();
       it != unwatched_.end(); ++it) {
    handlers_.erase(std::remove(handlers_.begin(), handlers_.end(), *it),
                    handlers_.end());
  }
  unwatched_.clear();
}
