#pragma once

#include <QDebug>
#include <QString>
#include <QtGlobal>

namespace skygate::testsupport {

enum class PerformanceBudgetDecision {
    WithinBudget,
    AdvisoryOverBudget,
    StrictOverBudget
};

[[nodiscard]] inline bool isStrictPerformanceGuardMode()
{
    const QString mode = QString::fromUtf8(qgetenv("SKYGATE_PERFORMANCE_GUARD_MODE")).trimmed().toLower();
    const QString legacyStrict = QString::fromUtf8(qgetenv("SKYGATE_STRICT_PERFORMANCE_GUARDS")).trimmed().toLower();
    return mode == QStringLiteral("strict")
           || (!legacyStrict.isEmpty() && legacyStrict != QStringLiteral("0") && legacyStrict != QStringLiteral("false")
               && legacyStrict != QStringLiteral("no") && legacyStrict != QStringLiteral("off"));
}

[[nodiscard]] inline QString performanceMetricMessage(
    const qint64 elapsedMs, const qint64 budgetMs, const char* operationName, const bool strictMode
)
{
    const qint64 deltaMs = budgetMs - elapsedMs;
    const double budgetPercent =
        budgetMs > 0 ? (static_cast<double>(elapsedMs) * 100.0 / static_cast<double>(budgetMs)) : 0.0;
    return QStringLiteral(
               "perf %1: %2 elapsed=%3 ms budget=%4 ms delta=%5 ms budget_used=%6% "
               "strict=%7"
    )
        .arg(elapsedMs < budgetMs ? QStringLiteral("within-budget") : QStringLiteral("over-budget"))
        .arg(QString::fromUtf8(operationName))
        .arg(elapsedMs)
        .arg(budgetMs)
        .arg(deltaMs)
        .arg(QString::number(budgetPercent, 'f', 1))
        .arg(strictMode ? QStringLiteral("on") : QStringLiteral("off"));
}

[[nodiscard]] inline PerformanceBudgetDecision
performanceBudgetDecision(const qint64 elapsedMs, const qint64 budgetMs, const bool strictMode) noexcept
{
    if (elapsedMs < budgetMs) {
        return PerformanceBudgetDecision::WithinBudget;
    }
    return strictMode ? PerformanceBudgetDecision::StrictOverBudget : PerformanceBudgetDecision::AdvisoryOverBudget;
}

[[nodiscard]] inline QString strictPerformanceBudgetHint(const QString& message)
{
    return QStringLiteral(
               "%1; set SKYGATE_PERFORMANCE_GUARD_MODE=strict or "
               "SKYGATE_STRICT_PERFORMANCE_GUARDS=1 to make advisory budgets fail"
    )
        .arg(message);
}

[[nodiscard]] inline PerformanceBudgetDecision
reportPerformanceBudget(const qint64 elapsedMs, const qint64 budgetMs, const char* operationName)
{
    const bool strictMode = isStrictPerformanceGuardMode();
    const QString message = performanceMetricMessage(elapsedMs, budgetMs, operationName, strictMode);
    const PerformanceBudgetDecision decision = performanceBudgetDecision(elapsedMs, budgetMs, strictMode);

    if (decision == PerformanceBudgetDecision::WithinBudget) {
        qInfo().noquote() << message;
        return decision;
    }

    qInfo().noquote() << strictPerformanceBudgetHint(message);
    return decision;
}

}  // namespace skygate::testsupport
