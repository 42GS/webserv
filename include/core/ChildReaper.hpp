/**
 * @file  ChildReaper.hpp
 * @brief Collecting finished CGI children.
 *        끝난 CGI 자식 회수.
 */
#ifndef CORE_CHILDREAPER_HPP
#define CORE_CHILDREAPER_HPP

#include <sys/types.h>

#include <vector>

/**
 * @brief Owns the pids of CGI children whose output is no longer needed
 *        and collects them with waitpid() (design 4).
 *        출력이 더는 필요 없는 CGI 자식의 pid를 맡아 waitpid()로 회수한다
 *        (설계 4절).
 *
 * A child that is not collected stays a zombie (wiki concepts/cgi
 * "자식 회수"). The exit status is not read, so the waitpid() macros
 * are not needed (wiki analysis/subject-vs-evaluation 1.9).
 * 회수하지 않은 자식은 좀비로 남는다 (위키 concepts/cgi "자식 회수").
 * 종료 상태는 읽지 않으므로 waitpid() 매크로가 필요 없다 (위키
 * analysis/subject-vs-evaluation 1.9절).
 */
class ChildReaper {
 public:
  /** @brief An empty reaper. / 비어 있는 reaper. */
  ChildReaper();

  /**
   * @brief Kills and waits for the children still held.
   *        아직 가진 자식을 죽이고 기다린다.
   */
  ~ChildReaper();

  /**
   * @brief Takes a child to collect.
   *        회수할 자식을 넘겨받는다.
   * @param[in] pid Child that was killed or whose output ended.
   *                죽였거나 출력이 끝난 자식.
   */
  void adopt(pid_t pid);

  /**
   * @brief waitpid(pid, WNOHANG) on each child; drops the collected ones.
   *        자식마다 waitpid(pid, WNOHANG)을 부르고 회수된 것은 뺀다.
   *
   * Never blocks. -1 also drops the pid: the child is gone or not ours
   * (errno is not read).
   * 멈추지 않는다. -1도 pid를 뺀다. 자식이 이미 없거나 우리 자식이 아니다
   * (errno는 읽지 않는다).
   */
  void reap();

 private:
  /// @cond
  // Not copyable: declared and never defined (C++98 has no "= delete").
  // 복사하지 않는다: 선언만 하고 정의하지 않는다 (C++98에는 "= delete"가 없다).
  ChildReaper(const ChildReaper&);
  ChildReaper& operator=(const ChildReaper&);
  /// @endcond

  /** Children to collect. / 회수할 자식. */
  std::vector<pid_t> children_;
};

#endif
