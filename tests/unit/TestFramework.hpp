/**
 * @file  TestFramework.hpp
 * @brief Minimal test runner: each test is a function registered with
 *        TEST_CASE.
 *        최소 테스트 실행기: 테스트마다 TEST_CASE로 등록한 함수 하나다.
 */
#ifndef TESTS_UNIT_TESTFRAMEWORK_HPP
#define TESTS_UNIT_TESTFRAMEWORK_HPP

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

/** A test body. / 테스트 본문. */
typedef void (*TestFunction)();

/**
 * @brief A registered test.
 *        등록한 테스트.
 */
struct TestCase {
  /** Name printed in the report. / 보고에 찍는 이름. */
  const char* name;
  /** Body. / 본문. */
  TestFunction function;
};

/**
 * @brief Every test, in registration order.
 *        모든 테스트, 등록 순서대로.
 */
std::vector<TestCase>& registeredTests();

/**
 * @brief Counts and prints a failed check.
 *        실패한 검사를 세고 출력한다.
 * @param[in] file    Source file of the check. / 검사가 있는 소스 파일.
 * @param[in] line    Line of the check. / 검사가 있는 줄.
 * @param[in] message What failed. / 실패한 내용.
 */
void reportFailure(const char* file, int line, const std::string& message);

/**
 * @brief Registers a test when constructed, before main() runs.
 *        생성될 때, 즉 main() 전에 테스트를 등록한다.
 */
struct TestRegistrar {
  /**
   * @brief Adds the test to registeredTests().
   *        테스트를 registeredTests()에 넣는다.
   * @param[in] name     Test name. / 테스트 이름.
   * @param[in] function Test body. / 테스트 본문.
   */
  TestRegistrar(const char* name, TestFunction function) {
    TestCase test;
    test.name = name;
    test.function = function;
    registeredTests().push_back(test);
  }
};

/**
 * @brief Defines and registers a test; the body follows in braces.
 *        테스트를 정의하고 등록한다. 본문은 뒤의 중괄호에 쓴다.
 */
#define TEST_CASE(name)                               \
  static void name();                                 \
  static TestRegistrar registrar_##name(#name, name); \
  static void name()

/**
 * @brief Fails the test if the condition is false; the test goes on.
 *        조건이 거짓이면 테스트를 실패로 기록하고, 테스트는 계속한다.
 */
#define CHECK(condition)                                             \
  do {                                                               \
    if (!(condition)) reportFailure(__FILE__, __LINE__, #condition); \
  } while (0)

/**
 * @brief Where a check was written; keeps checkEqual within four
 *        parameters.
 *        검사를 쓴 위치. checkEqual의 매개변수를 넷 이하로 유지한다.
 */
struct CheckSite {
  /**
   * @brief Stores the location.
   *        위치를 저장한다.
   * @param[in] sourceFile Source file. / 소스 파일.
   * @param[in] sourceLine Line. / 줄.
   * @param[in] text       Checked expression as written. / 쓴 그대로의 검사
   *                       식.
   */
  CheckSite(const char* sourceFile, int sourceLine, const char* text)
      : file(sourceFile), line(sourceLine), expression(text) {}

  /** Source file. / 소스 파일. */
  const char* file;
  /** Line. / 줄. */
  int line;
  /** Checked expression. / 검사 식. */
  const char* expression;
};

/**
 * @brief Fails the test unless the two values are equal.
 *        두 값이 같지 않으면 테스트를 실패로 기록한다.
 *
 * A function, not a macro body, so each argument is evaluated exactly once
 * and the message shows the value that was actually compared.
 * 매크로 본문이 아니라 함수라서, 인자를 정확히 한 번씩 계산하고 메시지에
 * 실제로 비교한 값을 보여 준다.
 *
 * @param[in] expected Expected value. / 기대값.
 * @param[in] actual   Actual value. / 실제값.
 * @param[in] site     Where the check was written. / 검사를 쓴 위치.
 */
template <typename Expected, typename Actual>
void checkEqual(const Expected& expected, const Actual& actual,
                const CheckSite& site) {
  if (expected == actual) return;
  std::ostringstream message;
  message << site.expression << ": got " << actual << ", expected " << expected;
  reportFailure(site.file, site.line, message.str());
}

/**
 * @brief checkEqual() at the place of the macro.
 *        매크로를 쓴 자리의 checkEqual().
 */
#define CHECK_EQ(expected, actual) \
  checkEqual((expected), (actual), CheckSite(__FILE__, __LINE__, #actual))

#endif
