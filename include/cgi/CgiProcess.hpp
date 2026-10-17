/**
 * @file  CgiProcess.hpp
 * @brief One running CGI script.
 *        실행 중인 CGI 스크립트 하나.
 */
#ifndef CGI_CGIPROCESS_HPP
#define CGI_CGIPROCESS_HPP

#include <sys/types.h>

#include <ctime>
#include <string>
#include <vector>

#include "cgi/CgiPipes.hpp"
#include "core/ChildReaper.hpp"
#include "core/IoHandler.hpp"

/**
 * @brief What to run: the program, its directory, script and environment.
 *        실행할 것: 프로그램, 디렉토리, 스크립트, 환경.
 */
struct CgiLaunch {
  /** Program from the "cgi" directive. / "cgi" 지시어의 프로그램. */
  std::string interpreter;
  /** Directory of the script, to run in. / 실행할 디렉토리 (스크립트의 것). */
  std::string directory;
  /**
   * @brief Script file name, the program's argument.
   *        프로그램 인자인 스크립트 이름.
   */
  std::string scriptFile;
  /** "NAME=value" meta-variables. / "NAME=value" 메타변수. */
  std::vector<std::string> environment;
};

/**
 * @brief The event loop and the reaper, which every CGI process needs.
 *        모든 CGI 프로세스에 필요한 이벤트 루프와 reaper.
 */
struct CgiServices {
  /**
   * @brief Bundles the two; both are kept by reference.
   *        둘을 묶는다. 둘 다 참조로 보관한다.
   * @param[in] loop      Event loop. / 이벤트 루프.
   * @param[in] collector Collects children. / 자식을 회수한다.
   */
  CgiServices(IoRegistry& loop, ChildReaper& collector)
      : registry(loop), reaper(collector) {}

  /** Event loop. / 이벤트 루프. */
  IoRegistry& registry;
  /** Collects children. / 자식을 회수한다. */
  ChildReaper& reaper;
};

/**
 * @brief Runs a script with fork() and execve(), feeds it the body and
 *        collects its output through the event loop (design 9).
 *        fork()와 execve()로 스크립트를 실행하고, 본문을 넘기고, 출력을
 *        이벤트 루프로 모은다 (설계 9절).
 *
 * The owner checks isDone() and hasTimedOut() once per loop iteration,
 * outside event dispatch. Deleting the process kills a child that is
 * still running; every child goes to the ChildReaper.
 * 소유자는 루프를 한 바퀴 돌 때마다, 이벤트 분배 밖에서 isDone()과
 * hasTimedOut()을 확인한다. 프로세스를 지우면 아직 실행 중인 자식을
 * 죽인다. 모든 자식은 ChildReaper로 간다.
 */
class CgiProcess {
 public:
  /**
   * @brief Prepares a process; nothing runs until start().
   *        프로세스를 준비한다. start() 전에는 아무것도 실행하지 않는다.
   * @param[in] launch   What to run. / 실행할 것.
   * @param[in] body     Request body; kept by reference and must outlive
   *                     the process.
   *                     요청 본문. 참조로 보관하며 프로세스보다 오래 살아야
   *                     한다.
   * @param[in] services Event loop and reaper; kept by reference.
   *                     이벤트 루프와 reaper. 참조로 보관한다.
   */
  CgiProcess(const CgiLaunch& launch, const std::string& body,
             const CgiServices& services);
  /**
   * @brief Closes the pipes; kills a child whose output is not finished.
   *        pipe를 닫는다. 출력이 끝나지 않은 자식은 죽인다.
   */
  ~CgiProcess();

  /**
   * @brief Creates the pipes and the child, and starts polling them.
   *        pipe와 자식을 만들고 poll을 시작한다.
   * @param[in] now Start time, for the timeout. / 시간 초과를 잴 시작 시각.
   * @return False if pipe() or fork() failed; nothing is left open.
   *         pipe()나 fork()가 실패하면 false. 열린 것은 남기지 않는다.
   */
  bool start(std::time_t now);

  /** @brief True once the output ended. / 출력이 끝났으면 true. */
  bool isDone() const;
  /**
   * @brief True if the output is unusable: a read error or more than
   *        limits::kMaxCgiOutputBytes.
   *        출력을 쓸 수 없으면 true: 읽기 오류나
   *        limits::kMaxCgiOutputBytes 초과.
   */
  bool failed() const;
  /** @brief Everything the script wrote. / 스크립트가 쓴 전부. */
  const std::string& output() const;
  /**
   * @brief True if the output has not ended within
   *        limits::kCgiTimeoutSeconds.
   *        limits::kCgiTimeoutSeconds 안에 출력이 끝나지 않았으면 true.
   * @param[in] now Current time. / 현재 시각.
   * @return Whether it timed out. / 시간을 넘었는지.
   */
  bool hasTimedOut(std::time_t now) const;

 private:
  /// @cond
  // Not copyable: declared and never defined (C++98 has no "= delete").
  // 복사하지 않는다: 선언만 하고 정의하지 않는다 (C++98에는 "= delete"가 없다).
  CgiProcess(const CgiProcess&);
  CgiProcess& operator=(const CgiProcess&);
  /// @endcond

  /**
   * @brief In the child: connects the pipes, closes every other inherited
   *        descriptor, enters the script directory and runs the program.
   *        자식에서: pipe를 잇고, 상속받은 다른 디스크립터를 모두 닫고,
   *        스크립트 디렉토리로 가서 프로그램을 실행한다.
   * @param[in] input  Read end of the input pipe. / 입력 pipe의 읽는 쪽.
   * @param[in] output Write end of the output pipe. / 출력 pipe의 쓰는 쪽.
   */
  void runChild(int input, int output);

  /** What to run. / 실행할 것. */
  CgiLaunch launch_;
  /** Request body. / 요청 본문. */
  const std::string& body_;
  /** Event loop. / 이벤트 루프. */
  IoRegistry& registry_;
  /** Collects the child. / 자식을 회수한다. */
  ChildReaper& reaper_;
  /** Child, -1 before start(). / 자식. start() 전에는 -1. */
  pid_t pid_;
  /** Start time. / 시작 시각. */
  std::time_t startedAt_;
  /** Writes the body; 0 before start(). / 본문을 쓴다. start() 전에는 0. */
  CgiStdin* stdin_;
  /** Reads the output; 0 before start(). / 출력을 읽는다. start() 전에는 0. */
  CgiStdout* stdout_;
};

#endif
