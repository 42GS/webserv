#ifndef TESTS_UNIT_TESTFRAMEWORK_HPP
#define TESTS_UNIT_TESTFRAMEWORK_HPP

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Minimal test runner: each test is a function registered with TEST_CASE.
typedef void (*TestFunction)();

struct TestCase {
    const char*  name;
    TestFunction function;
};

std::vector<TestCase>& registeredTests();
void reportFailure(const char* file, int line, const std::string& message);

struct TestRegistrar {
    TestRegistrar(const char* name, TestFunction function)
    {
        TestCase test;
        test.name = name;
        test.function = function;
        registeredTests().push_back(test);
    }
};

#define TEST_CASE(name)                                         \
    static void name();                                         \
    static TestRegistrar registrar_##name(#name, name);         \
    static void name()

#define CHECK(condition)                                        \
    do {                                                        \
        if (!(condition))                                       \
            reportFailure(__FILE__, __LINE__, #condition);      \
    } while (0)

#define CHECK_EQ(expected, actual)                              \
    do {                                                        \
        if (!((expected) == (actual))) {                        \
            std::ostringstream message_;                        \
            message_ << #actual << " == " << (actual)           \
                     << ", expected " << (expected);            \
            reportFailure(__FILE__, __LINE__, message_.str());  \
        }                                                       \
    } while (0)

#endif
