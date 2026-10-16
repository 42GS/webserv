#ifndef HTTP_CONNECTION_HPP
#define HTTP_CONNECTION_HPP

#include <cstddef>
#include <ctime>
#include <string>

#include "core/IoHandler.hpp"
#include "core/ListenContext.hpp"
#include "http/RequestParser.hpp"

// One client socket. Owns the fd and closes it in the destructor.
//
// Step 3 of the implementation plan: parses the request, answers errors
// with their status code, answers a parsed request with a fixed 200, and
// closes after every response. Routing and real responses come in step 4,
// keep-alive in step 5.
class Connection : public IoHandler {
 public:
  Connection(int fd, const ListenContext& where, std::time_t now);
  ~Connection();

  int fd() const;
  short wantedEvents() const;
  void onEvent(short revents);
  void onFailure();

  // Answers 408 or closes when the current state has lasted too long.
  void checkTimeout(std::time_t now);
  bool isClosed() const;

 private:
  enum State { READ_HEAD, READ_BODY, WRITE, CLOSED };

  Connection(const Connection&);
  Connection& operator=(const Connection&);

  void receiveOnce();
  void sendOnce();
  void parseInput();
  std::size_t bodyLimitFor(const Request& request) const;
  void startResponse(const std::string& bytes);
  void startErrorResponse(int status);
  void changeState(State next);

  int fd_;
  ListenContext where_;
  State state_;
  RequestParser parser_;
  std::string inBuffer_;
  std::string outBuffer_;
  std::string::size_type outSent_;
  std::time_t now_;         // last time seen by checkTimeout or the constructor
  std::time_t stateSince_;  // when the current state began
  std::time_t lastProgress_;  // last state change or successful I/O
};

#endif
