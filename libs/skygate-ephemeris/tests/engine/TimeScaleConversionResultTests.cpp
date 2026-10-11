#include "engine/TimeScaleConversionDiagnostics.hpp"
#include "engine/TimeScaleConversionResult.hpp"
#include "time/AstronomicalEpoch.hpp"

#include <QtTest/QtTest>

#include <string>

namespace {

[[nodiscard]] skygate::core::AstronomicalEpoch makeEpoch()
{
    return {
        .julianDatePart1 = 2460000.5,
        .julianDatePart2 = 1.25,
        .timeScale = skygate::core::TimeScale::Tt,
    };
}

}  // namespace

class TimeScaleConversionResultTests final : public QObject {
    Q_OBJECT

private slots:
    void successAppliesDefaultsAndNormalizesEpoch();
    void successKeepsExplicitStatusAndDiagnostic();
    void failureRetargetsEpochTimeScale();
    void mergeCombinesMasksAndPromotesDegraded();
    void mergeKeepsFailedResult();
};

void TimeScaleConversionResultTests::successAppliesDefaultsAndNormalizesEpoch()
{
    using namespace skygate::ephemeris;

    const TimeScaleConversionResult result = TimeScaleConversionResult::success(makeEpoch());
    QCOMPARE(result.status, TimeScaleConversionStatus::Valid);
    QCOMPARE(result.warningCodeMask, 0U);
    QCOMPARE(result.diagnosticText, std::string("Time-scale conversion succeeded."));
    QCOMPARE(result.epoch.julianDatePart1, 2460001.0);
    QCOMPARE(result.epoch.julianDatePart2, 0.75);
    QCOMPARE(result.epoch.timeScale, skygate::core::TimeScale::Tt);
    QVERIFY(result.isSuccess());
}

void TimeScaleConversionResultTests::successKeepsExplicitStatusAndDiagnostic()
{
    using namespace skygate::ephemeris;

    const TimeScaleConversionResult result = TimeScaleConversionResult::success(
        makeEpoch(),
        TimeScaleConversionStatus::Degraded,
        TimeScaleConversionDiagnostics::warningMask(TimeScaleConversionWarningCode::DeltaTFallbackApplied),
        "custom diagnostic"
    );
    QCOMPARE(result.status, TimeScaleConversionStatus::Degraded);
    QCOMPARE(
        result.warningCodeMask,
        TimeScaleConversionDiagnostics::warningMask(TimeScaleConversionWarningCode::DeltaTFallbackApplied)
    );
    QCOMPARE(result.diagnosticText, std::string("custom diagnostic"));
}

void TimeScaleConversionResultTests::failureRetargetsEpochTimeScale()
{
    using namespace skygate::ephemeris;

    const TimeScaleConversionResult result =
        TimeScaleConversionResult::failure(makeEpoch(), skygate::core::TimeScale::Utc, "failed");
    QCOMPARE(result.status, TimeScaleConversionStatus::Failed);
    QCOMPARE(result.epoch.timeScale, skygate::core::TimeScale::Utc);
    QCOMPARE(result.epoch.julianDatePart1, 2460000.5);
    QCOMPARE(result.epoch.julianDatePart2, 1.25);
    QCOMPARE(result.warningCodeMask, 0U);
    QCOMPARE(result.diagnosticText, std::string("failed"));
    QVERIFY(!result.isSuccess());
}

void TimeScaleConversionResultTests::mergeCombinesMasksAndPromotesDegraded()
{
    using namespace skygate::ephemeris;

    TimeScaleConversionResult result = TimeScaleConversionResult::success(makeEpoch());
    TimeScaleConversionResult::mergeWarnings(
        result,
        TimeScaleConversionStatus::Degraded,
        TimeScaleConversionDiagnostics::warningMask(TimeScaleConversionWarningCode::LeapSecondFallbackApplied)
    );
    QCOMPARE(result.status, TimeScaleConversionStatus::Degraded);
    QCOMPARE(
        result.warningCodeMask,
        TimeScaleConversionDiagnostics::warningMask(TimeScaleConversionWarningCode::LeapSecondFallbackApplied)
    );
}

void TimeScaleConversionResultTests::mergeKeepsFailedResult()
{
    using namespace skygate::ephemeris;

    TimeScaleConversionResult result =
        TimeScaleConversionResult::failure(makeEpoch(), skygate::core::TimeScale::Tt, "failed");
    TimeScaleConversionResult::mergeWarnings(
        result,
        TimeScaleConversionStatus::Degraded,
        TimeScaleConversionDiagnostics::warningMask(TimeScaleConversionWarningCode::InvalidInput)
    );
    QCOMPARE(result.status, TimeScaleConversionStatus::Failed);
    QCOMPARE(
        result.warningCodeMask,
        TimeScaleConversionDiagnostics::warningMask(TimeScaleConversionWarningCode::InvalidInput)
    );
}

QTEST_APPLESS_MAIN(TimeScaleConversionResultTests)

#include "TimeScaleConversionResultTests.moc"
