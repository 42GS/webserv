/**
 * @file  ChildReaper.cpp
 * @brief Child collection with waitpid().
 *        waitpid()로 자식 회수.
 */
#include "core/ChildReaper.hpp"

#include <signal.h>
#include <sys/wait.h>

ChildReaper::ChildReaper() {}

// At shutdown waiting is fine: SIGKILL cannot be caught (wiki
// concepts/cgi section 5), so each wait ends soon.
// 종료할 때는 기다려도 된다. SIGKILL은 잡을 수 없으므로 (위키
// concepts/cgi 5절) 기다림이 곧 끝난다.
ChildReaper::~ChildReaper() {
  for (std::size_t i = 0; i < children_.size(); ++i) {
    static_cast<void>(kill(children_[i], SIGKILL));
    int status = 0;
    static_cast<void>(waitpid(children_[i], &status, 0));
  }
}

void ChildReaper::adopt(pid_t pid) { children_.push_back(pid); }

void ChildReaper::reap() {
  std::vector<pid_t> running;
  for (std::size_t i = 0; i < children_.size(); ++i) {
    int status = 0;
    if (waitpid(children_[i], &status, WNOHANG) == 0)
      running.push_back(children_[i]);
  }
  children_.swap(running);
}
