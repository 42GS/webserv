#include <exception>

#include "unit/TestFramework.hpp"

namespace {

int g_failures = 0;

// No file:line here: the throw site is unknown to the runner.
void reportUncaught(const std::string& description) {
  ++g_failures;
  std::cout << "  FAIL uncaught exception: " << description << std::endl;
}

// An exception that escapes a test fails that test only; the rest still run.
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
