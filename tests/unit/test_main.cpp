#include "unit/TestFramework.hpp"

namespace {

int g_failures = 0;

}  // namespace

std::vector<TestCase>& registeredTests()
{
    static std::vector<TestCase> tests;
    return tests;
}

void reportFailure(const char* file, int line, const std::string& message)
{
    ++g_failures;
    std::cout << "  FAIL " << file << ":" << line << ": " << message << std::endl;
}

int main()
{
    const std::vector<TestCase>& tests = registeredTests();
    int failedTests = 0;
    for (std::size_t i = 0; i < tests.size(); ++i) {
        const int before = g_failures;
        tests[i].function();
        if (g_failures != before) {
            ++failedTests;
            std::cout << "[FAIL] " << tests[i].name << std::endl;
        } else {
            std::cout << "[ OK ] " << tests[i].name << std::endl;
        }
    }
    std::cout << tests.size() - failedTests << "/" << tests.size() << " tests passed" << std::endl;
    return failedTests == 0 ? 0 : 1;
}
