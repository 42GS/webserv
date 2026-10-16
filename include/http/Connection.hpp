#ifndef HTTP_CONNECTION_HPP
#define HTTP_CONNECTION_HPP

#include <ctime>
#include <string>

#include "core/IoHandler.hpp"

// One client socket. Owns the fd and closes it in the destructor.
//
// Step 2 of the implementation plan: reads until the end of the request
// head, answers a fixed 200 response and closes. Request parsing and real
// responses replace the fixed answer in later steps.
class Connection : public IoHandler {
 public:
  Connection(int fd, std::time_t now);
  ~Connection();

  int fd() const;
  short wantedEvents() const;
  void onEvent(short revents);
  void onFailure();

  // Closes the connection when the current state has lasted too long.
  void checkTimeout(std::time_t now);
  bool isClosed() const;

 private:
  enum State { READ_HEAD, WRITE, CLOSED };

  Connection(const Connection&);
  Connection& operator=(const Connection&);

  void receiveOnce();
  void sendOnce();
  void startResponse();
  void changeState(State next);

  int fd_;
  State state_;
  std::string inBuffer_;
  std::string outBuffer_;
  std::string::size_type outSent_;
  std::time_t now_;  // last time seen by checkTimeout or the constructor
  std::time_t lastProgress_;
};

#endif
