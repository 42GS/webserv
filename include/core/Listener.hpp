/**
 * @file  Listener.hpp
 * @brief Listen sockets and the receiver of accepted connections.
 *        listen 소켓과, accept한 연결을 받는 쪽.
 */
#ifndef CORE_LISTENER_HPP
#define CORE_LISTENER_HPP

#include <string>

#include "config/ServerConfig.hpp"
#include "core/IoHandler.hpp"
#include "core/ListenContext.hpp"

/**
 * @brief Receives sockets accepted by a Listener.
 *        Listener가 accept한 소켓을 받는다.
 */
class AcceptHandler {
 public:
  virtual ~AcceptHandler() {}

  /**
   * @brief Takes ownership of an accepted, non-blocking socket.
   *        accept한 non-blocking 소켓의 소유권을 넘겨받는다.
   * @param[in] clientFd The socket; the receiver must close it.
   *                     소켓. 받는 쪽이 닫아야 한다.
   * @param[in] where    The listening server block. / 연결을 받은 server 블록.
   */
  virtual void adopt(int clientFd, const ListenContext& where) = 0;
};

/**
 * @brief A non-blocking listen socket for one interface:port.
 *        인터페이스:포트 하나의 non-blocking listen 소켓.
 */
class Listener : public IoHandler {
 public:
  /**
   * @brief Prepares a listener; nothing is opened until open().
   *        listener를 준비한다. open() 전에는 아무것도 열지 않는다.
   * @param[in] server   Server block the connections belong to; kept by
   *                     reference.
   *                     연결이 속할 server 블록. 참조로 보관한다.
   * @param[in] addr     Address to listen on. / 들을 주소.
   * @param[in] acceptor Receiver of accepted sockets; kept by reference.
   *                     accept한 소켓을 받을 쪽. 참조로 보관한다.
   */
  Listener(const ServerConfig& server, const ListenAddr& addr,
           AcceptHandler& acceptor);

  /** @brief Closes the socket if it is open. / 소켓이 열려 있으면 닫는다. */
  ~Listener();

  /**
   * @brief socket, SO_REUSEADDR, bind, listen, then non-blocking.
   *        socket, SO_REUSEADDR, bind, listen 후 non-blocking으로 바꾼다.
   * @param[out] error On failure, a message naming the address and the
   *                   cause.
   *                   실패하면 주소와 원인을 담은 메시지.
   * @return False on failure. / 실패하면 false.
   */
  bool open(std::string& error);

  /**
   * @brief The listen socket, -1 before open().
   *        listen 소켓. open() 전에는 -1.
   */
  int fd() const;

  /** @brief Always POLLIN. / 항상 POLLIN. */
  short wantedEvents() const;

  /**
   * @brief Accepts one connection and hands it to the acceptor.
   *        연결 하나를 accept해서 acceptor에 넘긴다.
   * @param[in] revents Events from poll(). / poll()이 돌려준 이벤트.
   */
  void onEvent(short revents);

  /**
   * @brief Nothing to undo: a listener has no per-request state.
   *        되돌릴 것이 없다. listener에는 요청별 상태가 없다.
   */
  void onFailure();

 private:
  /// @cond
  // Not copyable: declared and never defined (C++98 has no "= delete").
  // 복사하지 않는다: 선언만 하고 정의하지 않는다 (C++98에는 "= delete"가 없다).
  Listener(const Listener&);
  Listener& operator=(const Listener&);
  /// @endcond

  /** Server block of accepted connections. / accept한 연결의 server 블록. */
  const ServerConfig& server_;
  /** Address listened on. / 듣는 주소. */
  ListenAddr addr_;
  /** Receiver of accepted sockets. / accept한 소켓을 받는 쪽. */
  AcceptHandler& acceptor_;
  /** Listen socket, -1 when closed. / listen 소켓, 닫혀 있으면 -1. */
  int fd_;
};

#endif
