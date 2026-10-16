#ifndef CORE_IOHANDLER_HPP
#define CORE_IOHANDLER_HPP

#include <ctime>

// Something registered with poll(): a listen socket, a client socket, or a
// CGI pipe. The event loop asks for the wanted events on every iteration,
// so POLLOUT is requested only while there is data to send.
class IoHandler {
 public:
  virtual ~IoHandler() {}
  virtual int fd() const = 0;
  virtual short wantedEvents() const = 0;
  // Called with the poll() result. Performs at most one read or write.
  virtual void onEvent(short revents) = 0;
  // Called when onEvent threw; the handler must end up closed.
  virtual void onFailure() = 0;
};

// Registration only; the registry never owns or deletes a handler.
class IoRegistry {
 public:
  virtual ~IoRegistry() {}
  virtual void watch(IoHandler* handler) = 0;
  virtual void unwatch(IoHandler* handler) = 0;
};

// Called by the event loop at the end of every iteration.
class LoopObserver {
 public:
  virtual ~LoopObserver() {}
  virtual void afterIteration(std::time_t now) = 0;
  virtual bool shouldStop() const = 0;
};

#endif
