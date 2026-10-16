#ifndef CORE_CLOCK_HPP
#define CORE_CLOCK_HPP

#include <ctime>
#include <string>

// Current time in whole seconds, read without time functions: the allowed
// function list has none (design decision 10). refresh() rewrites a small
// file and reads its modification time with stat().
class Clock {
 public:
  explicit Clock(const std::string& path);

  // Updates now(). Returns false (and keeps the old value) on failure.
  bool refresh();
  std::time_t now() const;

 private:
  std::string path_;
  std::time_t now_;
};

#endif
