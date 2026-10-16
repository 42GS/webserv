/**
 * @file  Clock.hpp
 * @brief Wall-clock time without time functions.
 *        시간 함수 없이 현재 시각을 얻는다.
 */
#ifndef CORE_CLOCK_HPP
#define CORE_CLOCK_HPP

#include <ctime>
#include <string>

/**
 * @brief Current time in whole seconds, read without time functions.
 *        시간 함수 없이 읽는 초 단위 현재 시각.
 *
 * The allowed function list has no time functions (design decision 10),
 * so refresh() rewrites a small file and reads its modification time with
 * stat().
 * 허용 함수 목록에 시간 함수가 없어서(설계 결정 10), refresh()는 작은
 * 파일을 다시 쓰고 stat()으로 그 수정 시각을 읽는다.
 */
class Clock {
 public:
  /**
   * @brief Uses @p path as the clock file; now() is 0 until refresh().
   *        @p path를 시계 파일로 쓴다. refresh() 전까지 now()는 0이다.
   * @param[in] path File to create and rewrite. / 만들고 다시 쓸 파일.
   */
  explicit Clock(const std::string& path);

  /**
   * @brief Updates now() from the clock file.
   *        시계 파일로 now()를 갱신한다.
   * @return False on failure; now() then keeps its old value.
   *         실패하면 false. 이때 now()는 이전 값을 유지한다.
   */
  bool refresh();

  /**
   * @brief Time of the last successful refresh().
   *        마지막으로 성공한 refresh()의 시각.
   */
  std::time_t now() const;

 private:
  /** Clock file. / 시계 파일. */
  std::string path_;
  /** Last time read. / 마지막으로 읽은 시각. */
  std::time_t now_;
};

#endif
