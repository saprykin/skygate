#pragma once

#include <cmath>
#include <initializer_list>
#include <iostream>
#include <string_view>

namespace skygate::core::tests {

inline bool expectTrue(const bool condition, const std::string_view message)
{
    if (condition) {
        return true;
    }

    std::cerr << "FAILED: " << message << '\n';
    return false;
}

inline bool expectNear(
    const double value,
    const double expected,
    const double tolerance,
    const std::string_view message
)
{
    return expectTrue(std::abs(value - expected) <= tolerance, message);
}

struct TestCase {
    std::string_view name;
    bool (*function)();
};

inline bool runAll(const std::initializer_list<TestCase>& tests)
{
    bool success = true;
    for (const auto& test : tests) {
        const bool testResult = test.function();
        success = testResult && success;
        if (!testResult) {
            std::cerr << "TEST GROUP FAILED: " << test.name << '\n';
        }
    }

    return success;
}

}  // namespace skygate::core::tests
