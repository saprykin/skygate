#include "engine/BitFlagSet.hpp"
#include "engine/TimeScaleConversionResult.hpp"
#include "engine/TimeScaleConversionWarningCode.hpp"
#include "time/AstronomicalEpoch.hpp"

#include <QtTest/QtTest>

#include <array>
#include <string>

namespace {

using skygate::ephemeris::TimeScaleConversionResult;
using skygate::ephemeris::TimeScaleConversionWarningCode;

[[nodiscard]] skygate::core::AstronomicalEpoch makeEpoch()
{
    return {
        .julianDatePart1 = 2460000.5,
        .julianDatePart2 = 1.25,
        .timeScale = skygate::core::TimeScale::Tt,
    };
}

[[nodiscard]] TimeScaleConversionResult::WarningMask
warningMaskFor(const TimeScaleConversionWarningCode::Type code) noexcept
{
    return TimeScaleConversionResult::WarningMask::fromBits(TimeScaleConversionResult::WarningMask::bit(code));
}

}  // namespace

class TimeScaleConversionResultTests final : public QObject {
    Q_OBJECT

private slots:
    void successAppliesDefaultsAndNormalizesEpoch();
    void successKeepsExplicitStatusAndDiagnostic();
    void failureRetargetsEpochTimeScale();
    void failureFromLookupCarriesLookupReport();
    void mergeCombinesMasksAndPromotesDegraded();
    void mergeKeepsFailedResult();
    void diagnosticsNameMatchesStatusCompanion();
};

void TimeScaleConversionResultTests::successAppliesDefaultsAndNormalizesEpoch()
{
    using namespace skygate::ephemeris;

    const TimeScaleConversionResult result = TimeScaleConversionResult::success(makeEpoch());
    QCOMPARE(result.status, TimeScaleConversionStatus::Type::Valid);
    QVERIFY(!result.warningCodeMask.hasAny());
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
        TimeScaleConversionStatus::Type::Degraded,
        warningMaskFor(TimeScaleConversionWarningCode::Type::DeltaTFallbackApplied),
        "custom diagnostic"
    );
    QCOMPARE(result.status, TimeScaleConversionStatus::Type::Degraded);
    QCOMPARE(result.warningCodeMask, warningMaskFor(TimeScaleConversionWarningCode::Type::DeltaTFallbackApplied));
    QCOMPARE(result.diagnosticText, std::string("custom diagnostic"));
}

void TimeScaleConversionResultTests::failureRetargetsEpochTimeScale()
{
    using namespace skygate::ephemeris;

    const TimeScaleConversionResult result =
        TimeScaleConversionResult::failure(makeEpoch(), skygate::core::TimeScale::Utc, "failed");
    QCOMPARE(result.status, TimeScaleConversionStatus::Type::Failed);
    QCOMPARE(result.epoch.timeScale, skygate::core::TimeScale::Utc);
    QCOMPARE(result.epoch.julianDatePart1, 2460000.5);
    QCOMPARE(result.epoch.julianDatePart2, 1.25);
    QVERIFY(!result.warningCodeMask.hasAny());
    QCOMPARE(result.diagnosticText, std::string("failed"));
    QVERIFY(!result.isSuccess());
}

void TimeScaleConversionResultTests::failureFromLookupCarriesLookupReport()
{
    using namespace skygate::ephemeris;

    const TimeScaleConversionResult result = TimeScaleConversionResult::failureFromLookup(
        makeEpoch(),
        skygate::core::TimeScale::Utc,
        warningMaskFor(TimeScaleConversionWarningCode::Type::LeapSecondTableMissing),
        "lookup failed"
    );
    QCOMPARE(result.status, TimeScaleConversionStatus::Type::Failed);
    QCOMPARE(result.epoch.timeScale, skygate::core::TimeScale::Utc);
    QCOMPARE(result.warningCodeMask, warningMaskFor(TimeScaleConversionWarningCode::Type::LeapSecondTableMissing));
    QVERIFY(result.hasWarning(TimeScaleConversionWarningCode::Type::LeapSecondTableMissing));
    QCOMPARE(result.diagnosticText, std::string("lookup failed"));
    QVERIFY(!result.isSuccess());
}

void TimeScaleConversionResultTests::mergeCombinesMasksAndPromotesDegraded()
{
    using namespace skygate::ephemeris;

    TimeScaleConversionResult result = TimeScaleConversionResult::success(makeEpoch());
    TimeScaleConversionResult::mergeWarnings(
        result,
        TimeScaleConversionStatus::Type::Degraded,
        warningMaskFor(TimeScaleConversionWarningCode::Type::LeapSecondFallbackApplied)
    );
    QCOMPARE(result.status, TimeScaleConversionStatus::Type::Degraded);
    QCOMPARE(result.warningCodeMask, warningMaskFor(TimeScaleConversionWarningCode::Type::LeapSecondFallbackApplied));
}

void TimeScaleConversionResultTests::mergeKeepsFailedResult()
{
    using namespace skygate::ephemeris;

    TimeScaleConversionResult result =
        TimeScaleConversionResult::failure(makeEpoch(), skygate::core::TimeScale::Tt, "failed");
    TimeScaleConversionResult::mergeWarnings(
        result,
        TimeScaleConversionStatus::Type::Degraded,
        warningMaskFor(TimeScaleConversionWarningCode::Type::InvalidInput)
    );
    QCOMPARE(result.status, TimeScaleConversionStatus::Type::Failed);
    QCOMPARE(result.warningCodeMask, warningMaskFor(TimeScaleConversionWarningCode::Type::InvalidInput));
}

void TimeScaleConversionResultTests::diagnosticsNameMatchesStatusCompanion()
{
    using namespace skygate::ephemeris;

    constexpr std::array statuses{
        TimeScaleConversionStatus::Type::Valid,
        TimeScaleConversionStatus::Type::Degraded,
        TimeScaleConversionStatus::Type::Failed,
    };

    for (const TimeScaleConversionStatus::Type status : statuses) {
        QCOMPARE(TimeScaleConversionDiagnostics::displayName(status), TimeScaleConversionStatus::displayName(status));
    }
}

QTEST_APPLESS_MAIN(TimeScaleConversionResultTests)

#include "TimeScaleConversionResultTests.moc"
