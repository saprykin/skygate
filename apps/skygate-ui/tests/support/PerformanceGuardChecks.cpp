#include "PerformanceGuardChecks.hpp"

#include "skygate/testsupport/PerformanceBudget.hpp"

#include <QtTest>

namespace skygate::ui::tests {

void PerformanceGuardChecks::verifyElapsedBelow(
    const qint64 elapsedMs, const qint64 budgetMs, const char* operationName
)
{
    const skygate::testsupport::PerformanceBudgetDecision decision =
        skygate::testsupport::reportPerformanceBudget(elapsedMs, budgetMs, operationName);
    QVERIFY2(
        decision != skygate::testsupport::PerformanceBudgetDecision::StrictOverBudget,
        qPrintable(
            skygate::testsupport::strictPerformanceBudgetHint(
                skygate::testsupport::performanceMetricMessage(elapsedMs, budgetMs, operationName, true)
            )
        )
    );
}

void PerformanceGuardChecks::verifyCounterDelta(
    const int before, const int after, const int expectedDelta, const char* counterName, const char* operationName
)
{
    const int actualDelta = after - before;
    const QString message = QStringLiteral("perf counter %1 during %2: expected delta=%3 actual delta=%4")
                                .arg(QString::fromUtf8(counterName))
                                .arg(QString::fromUtf8(operationName))
                                .arg(expectedDelta)
                                .arg(actualDelta);
    QVERIFY2(actualDelta == expectedDelta, qPrintable(message));
}

}  // namespace skygate::ui::tests
