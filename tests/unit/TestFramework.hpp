#ifndef TESTS_UNIT_TESTFRAMEWORK_HPP
#define TESTS_UNIT_TESTFRAMEWORK_HPP

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Minimal test runner: each test is a function registered with TEST_CASE.
typedef void (*TestFunction)();

struct TestCase {
  const char* name;
  TestFunction function;
};

std::vector<TestCase>& registeredTests();
void reportFailure(const char* file, int line, const std::string& message);

struct TestRegistrar {
  TestRegistrar(const char* name, TestFunction function) {
    TestCase test;
    test.name = name;
    test.function = function;
    registeredTests().push_back(test);
  }
};

#define TEST_CASE(name)                               \
  static void name();                                 \
  static TestRegistrar registrar_##name(#name, name); \
  static void name()

#define CHECK(condition)                                             \
  do {                                                               \
    if (!(condition)) reportFailure(__FILE__, __LINE__, #condition); \
  } while (0)

// Where a check was written; keeps checkEqual within four parameters.
struct CheckSite {
  CheckSite(const char* sourceFile, int sourceLine, const char* text)
      : file(sourceFile), line(sourceLine), expression(text) {}
  const char* file;
  int line;
  const char* expression;
};

// A function, not a macro body, so each argument is evaluated exactly once
// and the message shows the value that was actually compared.
template <typename Expected, typename Actual>
void checkEqual(const Expected& expected, const Actual& actual,
                const CheckSite& site) {
  if (expected == actual) return;
  std::ostringstream message;
  message << site.expression << ": got " << actual << ", expected " << expected;
  reportFailure(site.file, site.line, message.str());
}

#define CHECK_EQ(expected, actual) \
  checkEqual((expected), (actual), CheckSite(__FILE__, __LINE__, #actual))

#endif
