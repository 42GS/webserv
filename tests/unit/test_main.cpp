/**
 * @file  test_main.cpp
 * @brief Runs every registered test and reports the result.
 *        등록한 테스트를 모두 실행하고 결과를 보고한다.
 */
#include <exception>

#include "unit/TestFramework.hpp"

namespace {

/** Failed checks of all tests so far. / 모든 테스트의 실패한 검사 수. */
int g_failures = 0;

/**
 * @brief Counts and prints an exception that escaped a test.
 *        테스트 밖으로 나온 예외를 세고 출력한다.
 *
 * No file:line here: the throw site is unknown to the runner.
 * 여기에는 file:line이 없다. 실행기는 예외가 던져진 위치를 모른다.
 *
 * @param[in] description What was thrown. / 던져진 것.
 */
void reportUncaught(const std::string& description) {
  ++g_failures;
  std::cout << "  FAIL uncaught exception: " << description << std::endl;
}

/**
 * @brief Runs one test.
 *        테스트 하나를 실행한다.
 *
 * An exception that escapes a test fails that test only; the rest still run.
 * 테스트 밖으로 나온 예외는 그 테스트만 실패시키고, 나머지는 계속 실행한다.
 *
 * @param[in] test The test. / 테스트.
 */
void runGuarded(const TestCase& test) {
  try {
    test.function();
  } catch (const std::exception& error) {
    reportUncaught(error.what());
  } catch (...) {
    reportUncaught("not derived from std::exception");
  }
}

}  // namespace

std::vector<TestCase>& registeredTests() {
  static std::vector<TestCase> tests;
  return tests;
}

void reportFailure(const char* file, int line, const std::string& message) {
  ++g_failures;
  std::cout << "  FAIL " << file << ":" << line << ": " << message << std::endl;
}

/**
 * @brief Runs the tests in registration order.
 *        등록 순서대로 테스트를 실행한다.
 * @return 0 if every test passed, else 1.
 *         모든 테스트가 통과하면 0, 아니면 1.
 */
int main() {
  const std::vector<TestCase>& tests = registeredTests();
  int failedTests = 0;
  for (std::size_t i = 0; i < tests.size(); ++i) {
    const int before = g_failures;
    runGuarded(tests[i]);
    if (g_failures != before) {
      ++failedTests;
      std::cout << "[FAIL] " << tests[i].name << std::endl;
    } else {
      std::cout << "[ OK ] " << tests[i].name << std::endl;
    }
  }
  std::cout << tests.size() - failedTests << "/" << tests.size()
            << " tests passed" << std::endl;
  return failedTests == 0 ? 0 : 1;
}
