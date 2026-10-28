/**
 * @file  ConnectionServices.hpp
 * @brief Objects shared by every connection.
 *        모든 연결이 함께 쓰는 객체.
 */
#ifndef APP_CONNECTIONSERVICES_HPP
#define APP_CONNECTIONSERVICES_HPP

#include "cgi/CgiProcess.hpp"
#include "core/ChildReaper.hpp"
#include "core/IoHandler.hpp"
#include "core/Logger.hpp"

/**
 * @brief The event loop, the child reaper and the logs, owned by main and
 *        kept here by reference (keeps constructors within four
 *        parameters).
 *        main이 소유하는 이벤트 루프, 자식 reaper, 로그. 여기서는 참조로
 *        갖는다 (생성자 매개변수를 넷 이하로 유지한다).
 */
struct ConnectionServices {
  /**
   * @brief Bundles the three.
   *        셋을 묶는다.
   * @param[in] loop      Event loop. / 이벤트 루프.
   * @param[in] collector Collects children. / 자식을 회수한다.
   * @param[in] log       Access and error logs. / 접근 로그와 오류 로그.
   */
  ConnectionServices(IoRegistry& loop, ChildReaper& collector,
                     const Logger& log)
      : registry(loop), reaper(collector), logger(log) {}

  /**
   * @brief The part a CGI process needs.
   *        CGI 프로세스에 필요한 부분.
   */
  CgiServices cgi() const { return CgiServices(registry, reaper); }

  /** Event loop. / 이벤트 루프. */
  IoRegistry& registry;
  /** Collects children. / 자식을 회수한다. */
  ChildReaper& reaper;
  /** Access and error logs. / 접근 로그와 오류 로그. */
  const Logger& logger;
};

#endif
