#pragma once

#include <QtGlobal>

namespace skygate::ui::tests {

// Budget threshold and QtTest verification helpers shared by the performance
// guard suites.
class PerformanceGuardChecks final {
public:
    static constexpr qint64 kHighPrecisionLargeSceneBuildBudgetMs = 15000;

    static void verifyElapsedBelow(qint64 elapsedMs, qint64 budgetMs, const char* operationName);

    static void
    verifyCounterDelta(int before, int after, int expectedDelta, const char* counterName, const char* operationName);
};

}  // namespace skygate::ui::tests
