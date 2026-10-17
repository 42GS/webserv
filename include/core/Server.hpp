/**
 * @file  Server.hpp
 * @brief Owner of every listener and connection.
 *        모든 listener와 연결의 소유자.
 */
#ifndef CORE_SERVER_HPP
#define CORE_SERVER_HPP

#include <ctime>
#include <map>
#include <string>
#include <vector>

#include "config/ServerConfig.hpp"
#include "core/ChildReaper.hpp"
#include "core/IoHandler.hpp"
#include "core/Listener.hpp"

class Connection;

/**
 * @brief Owns every Listener and Connection.
 *        모든 Listener와 Connection을 소유한다.
 *
 * Registers them with the event loop, checks timeouts after each
 * iteration, and deletes connections once they are closed.
 * 이벤트 루프에 등록하고, 반복이 끝날 때마다 시간 초과를 검사하고,
 * 닫힌 연결을 지운다.
 */
class Server : public LoopObserver, public AcceptHandler {
 public:
  /**
   * @brief Prepares the server; no socket is opened yet.
   *        서버를 준비한다. 아직 소켓은 열지 않는다.
   * @param[in] configs  Loaded settings; kept by reference.
   *                     읽은 설정. 참조로 보관한다.
   * @param[in] registry Event loop; kept by reference.
   *                     이벤트 루프. 참조로 보관한다.
   * @param[in] reaper   Collects CGI children; kept by reference.
   *                     CGI 자식을 회수한다. 참조로 보관한다.
   * @param[in] now      Current time. / 현재 시각.
   */
  Server(const std::vector<ServerConfig>& configs, IoRegistry& registry,
         ChildReaper& reaper, std::time_t now);

  /**
   * @brief Closes every connection and listener.
   *        모든 연결과 listener를 닫는다.
   */
  ~Server();

  /**
   * @brief Opens every listen address.
   *        모든 listen 주소를 연다.
   *
   * Listeners opened so far are closed by the destructor.
   * 실패 전까지 연 listener는 소멸자가 닫는다.
   *
   * @param[out] error On failure, the message of the failed listener.
   *                   실패하면 실패한 listener의 메시지.
   * @return False on the first failure. / 처음 실패하면 false.
   */
  bool openListeners(std::string& error);

  /**
   * @brief Creates a Connection for an accepted socket.
   *        accept한 소켓으로 Connection을 만든다.
   *
   * If that fails (std::bad_alloc), the socket is closed.
   * 만들지 못하면(std::bad_alloc) 소켓을 닫는다.
   *
   * @param[in] clientFd Accepted socket, now owned by the server.
   *                     accept한 소켓. 이제 서버가 소유한다.
   * @param[in] where    The listening server block. / 연결을 받은 server 블록.
   */
  void adopt(int clientFd, const ListenContext& where);

  /**
   * @brief Checks timeouts, deletes closed connections and collects
   *        finished CGI children.
   *        시간 초과를 검사하고, 닫힌 연결을 지우고, 끝난 CGI 자식을
   *        회수한다.
   * @param[in] now Current time. / 현재 시각.
   */
  void afterIteration(std::time_t now);

  /**
   * @brief True once SIGINT or SIGTERM arrived.
   *        SIGINT나 SIGTERM이 오면 true.
   */
  bool shouldStop() const;

 private:
  /// @cond
  // Not copyable: declared and never defined (C++98 has no "= delete").
  // 복사하지 않는다: 선언만 하고 정의하지 않는다 (C++98에는 "= delete"가 없다).
  Server(const Server&);
  Server& operator=(const Server&);
  /// @endcond

  /**
   * @brief Unwatches, forgets and deletes a connection.
   *        연결의 감시를 풀고, 목록에서 빼고, 지운다.
   * @param[in] connection The connection. / 연결.
   */
  void closeConnection(Connection* connection);

  /** Loaded settings. / 읽은 설정. */
  const std::vector<ServerConfig>& configs_;
  /** Event loop. / 이벤트 루프. */
  IoRegistry& registry_;
  /** Collects CGI children. / CGI 자식을 회수한다. */
  ChildReaper& reaper_;
  /** Time of the last iteration. / 마지막 반복의 시각. */
  std::time_t now_;
  /**
   * @brief When paused listeners were last resumed.
   *        멈춘 listener를 마지막으로 다시 시도한 시각.
   */
  std::time_t listenersResumedAt_;
  /** Owned listeners. / 소유한 listener. */
  std::vector<Listener*> listeners_;
  /** Owned connections by socket. / 소켓별로 소유한 연결. */
  std::map<int, Connection*> connections_;
};

#endif
