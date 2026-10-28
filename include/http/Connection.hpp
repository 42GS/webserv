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
 * Reads requests one after another, lets RequestProcessor build each
 * response and sends them in order (keep-alive, pipelining). After a
 * response that ends the connection it closes, first draining the rest of
 * a request that was not read completely (concepts/connection-management).
 * 요청을 차례로 읽고, RequestProcessor가 만든 응답을 순서대로 보낸다
 * (keep-alive, pipelining). 연결을 끝내는 응답 뒤에는 닫는다. 요청을 다
 * 읽지 못했으면 남은 것을 먼저 읽어 버린다
 * (concepts/connection-management).
 */
class Connection : public IoHandler {
 public:
  /**
   * @brief Starts reading a request on an accepted socket.
   *        accept한 소켓에서 요청 읽기를 시작한다.
   * @param[in] fd       Non-blocking socket, now owned.
   *                     non-blocking 소켓. 이제 소유한다.
   * @param[in] where    The listening server block and both ends.
   *                     연결을 받은 server 블록과 두 끝.
   * @param[in] services For CGI and the logs; copied, and the copy refers
   *                     to the same objects.
   *                     CGI와 로그용. 복사하며, 복사본도 같은 객체를
   *                     가리킨다.
   * @param[in] now      Current time. / 현재 시각.
   */
  Connection(int fd, const ListenContext& where,
             const ConnectionServices& services, std::time_t now);

  /**
   * @brief Closes the socket and ends a CGI process still running.
   *        소켓을 닫고, 아직 실행 중인 CGI 프로세스를 끝낸다.
   */
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
   * @brief Once per loop iteration: answers 408 or closes when the
   *        current state has lasted too long, and follows a CGI process.
   *        루프 한 바퀴마다: 지금 상태가 너무 오래 이어지면 408로 답하거나
   *        닫고, CGI 프로세스를 살핀다.
   *
   * CGI results are taken here, outside event dispatch, so that a
   * finished process is never deleted inside its own pipe's onEvent().
   * CGI 결과는 이벤트 분배 밖인 여기서 받는다. 끝난 프로세스를 그 pipe의
   * onEvent() 안에서 지우는 일이 없게 하기 위해서다.
   *
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
    /**
     * @brief Waiting for a CGI script; its pipes are polled instead.
     *        CGI 스크립트를 기다리는 중. 대신 그 pipe를 poll한다.
     */
    WAIT_CGI,
    /** Sending the response. / 응답을 보내는 중. */
    WRITE,
    /**
     * @brief Discarding the rest of an unread request before closing.
     *        닫기 전에 다 읽지 않은 요청의 나머지를 버리는 중.
     */
    DRAIN,
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
   * @brief One recv(); the bytes are parsed, or discarded in DRAIN.
   *        recv() 한 번. 받은 바이트는 파싱하거나, DRAIN이면 버린다.
   */
  void receiveOnce();

  /**
   * @brief One send() of the pending response bytes.
   *        남은 응답 바이트를 send() 한 번.
   */
  void sendOnce();

  /**
   * @brief After the last byte of a response: the next request, DRAIN or
   *        CLOSED.
   *        응답의 마지막 바이트 뒤: 다음 요청, DRAIN, CLOSED 중 하나로 간다.
   */
  void finishResponse();

  /**
   * @brief Feeds inBuffer_ to the parser and acts on the result.
   *        inBuffer_를 파서에 넣고 결과에 따라 움직인다.
   */
  void parseInput();

  /**
   * @brief Acts on what processing a request led to.
   *        요청을 처리한 결과에 따라 움직인다.
   *
   * A local redirect is answered in place, up to
   * limits::kMaxInternalRedirects times, then 500 (design decision 6).
   * 로컬 리다이렉트는 그 자리에서 답하되 limits::kMaxInternalRedirects번까지,
   * 넘으면 500이다 (설계 결정 6).
   *
   * @param[in] outcome The outcome. / 결과.
   */
  void handleOutcome(const ProcessOutcome& outcome);

  /**
   * @brief Takes the result of a finished CGI process, or answers 504 once
   *        it times out.
   *        끝난 CGI 프로세스의 결과를 받거나, 시간을 넘으면 504로 답한다.
   */
  void followCgi();

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
   * @brief Starts sending an error page for @p status, for a request that
   *        was not read completely.
   *        다 읽지 못한 요청에 @p status의 오류 페이지 전송을 시작한다.
   * @param[in] status Status code. / 상태 코드.
   */
  void startErrorResponse(int status);

  /**
   * @brief Writes the response to the access log, with the request line
   *        the client sent.
   *        클라이언트가 보낸 요청 줄과 함께 응답을 접근 로그에 쓴다.
   * @param[in] response The response being sent. / 보내기 시작한 응답.
   */
  void logAccess(const Response& response) const;

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
  /** Client address, dotted. / 점 표기 클라이언트 주소. */
  std::string client_;
  /** Access and error logs. / 접근 로그와 오류 로그. */
  const Logger& logger_;
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
  /**
   * @brief Running CGI process, owned; 0 if none.
   *        실행 중인 CGI 프로세스. 소유하며 없으면 0.
   */
  CgiProcess* cgi_;
  /**
   * @brief The request being answered: the parsed one or redirected_.
   *        답하고 있는 요청: 파싱한 것이나 redirected_.
   */
  const Request* current_;
  /** Request of the last local redirect. / 마지막 로컬 리다이렉트의 요청. */
  Request redirected_;
  /** Local redirects for this request. / 이 요청의 로컬 리다이렉트 수. */
  int localRedirects_;
  /** Received bytes not yet parsed. / 받았지만 아직 파싱하지 않은 바이트. */
  std::string inBuffer_;
  /**
   * @brief Keep the connection after the response being sent.
   *        보내고 있는 응답 뒤에 연결을 유지한다.
   */
  bool keepAlive_;
  /**
   * @brief The request being answered was read completely; if not, DRAIN
   *        comes before closing.
   *        응답하고 있는 요청을 다 읽었다. 아니면 닫기 전에 DRAIN을 거친다.
   */
  bool requestConsumed_;
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
