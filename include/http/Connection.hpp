#ifndef HTTP_CONNECTION_HPP
#define HTTP_CONNECTION_HPP

#include <cstddef>
#include <ctime>
#include <string>

#include "app/RequestProcessor.hpp"
#include "core/IoHandler.hpp"
#include "core/ListenContext.hpp"
#include "http/RequestParser.hpp"
#include "http/ResponseStream.hpp"

// One client socket. Owns the fd and closes it in the destructor.
//
// Reads and parses one request, lets RequestProcessor build the response,
// sends it and closes. Keep-alive comes in step 5.
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
  void startResponse(const Response& response);
  void startErrorResponse(int status);
  void changeState(State next);
  bool hasTimedOut() const;

  int fd_;
  State state_;
  RequestParser parser_;
  RequestProcessor processor_;
  ResponseStream stream_;
  std::string inBuffer_;
  std::time_t now_;         // last time seen by checkTimeout or the constructor
  std::time_t stateSince_;  // when the current state began
  std::time_t lastProgress_;  // last state change or successful I/O
};

#endif
