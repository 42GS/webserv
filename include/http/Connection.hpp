/**
 * @file  Connection.hpp
 * @brief One client connection.
 *        클라이언트 연결 하나.
 */
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

/**
 * @brief One client socket. Owns the fd and closes it in the destructor.
 *        클라이언트 소켓 하나. fd를 소유하고 소멸자에서 닫는다.
 *
 * Reads and parses one request, lets RequestProcessor build the response,
 * sends it and closes. Keep-alive comes in step 5.
 * 요청 하나를 읽어 파싱하고, RequestProcessor가 만든 응답을 보낸 뒤
 * 닫는다. keep-alive는 5단계에서 넣는다.
 */
class Connection : public IoHandler {
 public:
  /**
   * @brief Starts reading a request on an accepted socket.
   *        accept한 소켓에서 요청 읽기를 시작한다.
   * @param[in] fd    Non-blocking socket, now owned. / non-blocking 소켓. 이제
   * 소유한다.
   * @param[in] where The listening server block. / 연결을 받은 server 블록.
   * @param[in] now   Current time. / 현재 시각.
   */
  Connection(int fd, const ListenContext& where, std::time_t now);

  /** @brief Closes the socket. / 소켓을 닫는다. */
  ~Connection();

  /** @brief The client socket. / 클라이언트 소켓. */
  int fd() const;

  /**
   * @brief POLLIN while reading, POLLOUT while writing, 0 when closed.
   *        읽는 중 POLLIN, 쓰는 중 POLLOUT, 닫히면 0.
   */
  short wantedEvents() const;

  /**
   * @brief Does one recv() or send(), or closes on an error or hang-up.
   *        recv()나 send()를 한 번 하거나, 오류·끊김이면 닫는다.
   * @param[in] revents Events from poll(). / poll()이 돌려준 이벤트.
   */
  void onEvent(short revents);

  /**
   * @brief Closes after an exception in onEvent().
   *        onEvent()에서 예외가 나면 닫는다.
   */
  void onFailure();

  /**
   * @brief Answers 408 or closes when the current state has lasted too
   *        long.
   *        지금 상태가 너무 오래 이어지면 408로 답하거나 닫는다.
   * @param[in] now Current time. / 현재 시각.
   */
  void checkTimeout(std::time_t now);

  /**
   * @brief True once the connection is finished; the owner deletes it.
   *        연결이 끝났으면 true. 소유자가 지운다.
   */
  bool isClosed() const;

 private:
  /**
   * @brief What the connection is doing.
   *        연결이 하고 있는 일.
   */
  enum State {
    /** Reading the request line and headers. / 요청 줄과 헤더를 읽는 중. */
    READ_HEAD,
    /** Reading the body. / 본문을 읽는 중. */
    READ_BODY,
    /** Sending the response. / 응답을 보내는 중. */
    WRITE,
    /** Finished. / 끝남. */
    CLOSED
  };

  /// @cond
  // Not copyable: declared and never defined (C++98 has no "= delete").
  // 복사하지 않는다: 선언만 하고 정의하지 않는다 (C++98에는 "= delete"가 없다).
  Connection(const Connection&);
  Connection& operator=(const Connection&);
  /// @endcond

  /**
   * @brief One recv() into inBuffer_, then parsing.
   *        inBuffer_로 recv() 한 번, 그다음 파싱.
   */
  void receiveOnce();

  /**
   * @brief One send() of the pending response bytes.
   *        남은 응답 바이트를 send() 한 번.
   */
  void sendOnce();

  /**
   * @brief Feeds inBuffer_ to the parser and acts on the result.
   *        inBuffer_를 파서에 넣고 결과에 따라 움직인다.
   */
  void parseInput();

  /**
   * @brief Starts sending a response.
   *        응답 전송을 시작한다.
   *
   * A file that vanished between routing and opening becomes a 500.
   * 라우팅과 열기 사이에 사라진 파일은 500이 된다.
   *
   * @param[in] response The response. / 응답.
   */
  void startResponse(const Response& response);

  /**
   * @brief Starts sending an error page for @p status.
   *        @p status의 오류 페이지 전송을 시작한다.
   * @param[in] status Status code. / 상태 코드.
   */
  void startErrorResponse(int status);

  /**
   * @brief Moves to another state and restarts its timers.
   *        다른 상태로 옮기고 시간 기준을 다시 잡는다.
   * @param[in] next New state. / 새 상태.
   */
  void changeState(State next);

  /**
   * @brief True if the timeout of the current state has passed.
   *        지금 상태의 제한 시간이 지났으면 true.
   */
  bool hasTimedOut() const;

  /** Client socket. / 클라이언트 소켓. */
  int fd_;
  /** Current state. / 현재 상태. */
  State state_;
  /** Request parser. / 요청 파서. */
  RequestParser parser_;
  /**
   * @brief Builds responses for this server block.
   *        이 server 블록의 응답을 만든다.
   */
  RequestProcessor processor_;
  /** Response being sent. / 보내고 있는 응답. */
  ResponseStream stream_;
  /** Received bytes not yet parsed. / 받았지만 아직 파싱하지 않은 바이트. */
  std::string inBuffer_;
  /**
   * @brief Last time seen by checkTimeout() or the constructor.
   *        checkTimeout()이나 생성자가 마지막으로 본 시각.
   */
  std::time_t now_;
  /** When the current state began. / 현재 상태가 시작된 시각. */
  std::time_t stateSince_;
  /**
   * @brief Last state change or successful I/O.
   *        마지막 상태 변경이나 성공한 I/O의 시각.
   */
  std::time_t lastProgress_;
};

#endif
