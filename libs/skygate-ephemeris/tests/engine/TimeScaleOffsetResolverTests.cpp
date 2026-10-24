#include "engine/IDeltaTProvider.hpp"
#include "engine/IEarthOrientationProvider.hpp"
#include "engine/ILeapSecondProvider.hpp"
#include "engine/LeapSecondOffsetResolver.hpp"
#include "engine/TableBackedLeapSecondProvider.hpp"
#include "engine/TimeScaleServiceOptions.hpp"
#include "engine/Ut1OffsetResolver.hpp"
#include "math/TimeConstants.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/CalendarTime.hpp"
#include "time/CivilDateTime.hpp"
#include "time/EphemerisDateRange.hpp"
#include "time/TimeScale.hpp"

#include <QtTest/QtTest>

#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

using skygate::core::AstronomicalEpoch;
using skygate::core::CalendarTime;
using skygate::core::CivilDateTime;
using skygate::core::TimeConstants;
using skygate::core::TimeScale;
using skygate::ephemeris::EphemerisDateRange;
using skygate::ephemeris::EphemerisTableDataStatus;
using skygate::ephemeris::IDeltaTProvider;
using skygate::ephemeris::IEarthOrientationProvider;
using skygate::ephemeris::ILeapSecondProvider;
using skygate::ephemeris::LeapSecondOffsetResolver;
using skygate::ephemeris::TableBackedLeapSecondProvider;
using skygate::ephemeris::TimeScaleConversionResult;
using skygate::ephemeris::TimeScaleConversionWarningCode;
using skygate::ephemeris::TimeScaleServiceOptions;
using skygate::ephemeris::Ut1OffsetResolver;

using ConversionStatus = skygate::ephemeris::TimeScaleConversionStatus::Type;

[[nodiscard]] AstronomicalEpoch
epochForDateTime(const int year, const int month, const int day, const int hour, const int minute, const int second)
{
    const auto epoch = CalendarTime::astronomicalEpochFromCivilDateTime(
        CivilDateTime{
            .astronomicalYear = year,
            .month = month,
            .day = day,
            .hour = hour,
            .minute = minute,
            .second = second,
            .timeScale = TimeScale::Utc,
        }
    );
    Q_ASSERT(epoch.has_value());
    return *epoch;
}

[[nodiscard]] AstronomicalEpoch epochForDate(const int year, const int month, const int day)
{
    return epochForDateTime(year, month, day, 0, 0, 0);
}

[[nodiscard]] AstronomicalEpoch taiEpochFor(const AstronomicalEpoch& utcEpoch, const double taiMinusUtcSeconds)
{
    return utcEpoch.addSeconds(taiMinusUtcSeconds, TimeScale::Tai);
}

[[nodiscard]] double secondsBetween(const AstronomicalEpoch& lhs, const AstronomicalEpoch& rhs) noexcept
{
    return ((lhs.julianDatePart1 - rhs.julianDatePart1) + (lhs.julianDatePart2 - rhs.julianDatePart2))
           * TimeConstants::kSecondsPerDay;
}

template <typename Lookup>
[[nodiscard]] bool hasWarning(const Lookup& lookup, const TimeScaleConversionWarningCode::Type code)
{
    return lookup.warningCodeMask.has(TimeScaleConversionResult::WarningMask::bit(code));
}

[[nodiscard]] ILeapSecondProvider::TableInfo leapSecondTableInfo(
    const EphemerisTableDataStatus status = EphemerisTableDataStatus::Available,
    std::optional<EphemerisDateRange> validityRange = std::nullopt
)
{
    ILeapSecondProvider::TableInfo info;
    info.version = "test";
    info.provenance = "unit test";
    info.status = status;
    info.validityRange = std::move(validityRange);
    return info;
}

[[nodiscard]] std::vector<ILeapSecondProvider::TableEntry> fixedLeapSecondEntries()
{
    return {
        {.effectiveUtcEpoch = epochForDate(1972, 1, 1), .taiMinusUtcSeconds = 10},
        {.effectiveUtcEpoch = epochForDate(1972, 7, 1), .taiMinusUtcSeconds = 11},
        {.effectiveUtcEpoch = epochForDate(2015, 7, 1), .taiMinusUtcSeconds = 36},
        {.effectiveUtcEpoch = epochForDate(2017, 1, 1), .taiMinusUtcSeconds = 37},
    };
}

[[nodiscard]] EphemerisDateRange leapSecondValidityRange()
{
    return {
        .id = "leap-seconds",
        .displayName = "Leap seconds",
        .start = epochForDate(1972, 1, 1),
        .end = epochForDate(2027, 1, 1),
    };
}

[[nodiscard]] std::shared_ptr<const ILeapSecondProvider> leapSecondProvider(
    ILeapSecondProvider::TableInfo tableInfo = leapSecondTableInfo(),
    std::vector<ILeapSecondProvider::TableEntry> entries = fixedLeapSecondEntries()
)
{
    return std::make_shared<TableBackedLeapSecondProvider>(std::move(tableInfo), std::move(entries));
}

class FixedEarthOrientationProvider final : public IEarthOrientationProvider {
public:
    FixedEarthOrientationProvider(
        IEarthOrientationProvider::DataInfo info, std::vector<IEarthOrientationProvider::TableEntry> entries
    )
        : m_info(std::move(info)), m_entries(std::move(entries))
    {
    }

    [[nodiscard]] const IEarthOrientationProvider::DataInfo& dataInfo() const noexcept override
    {
        return m_info;
    }

    [[nodiscard]] std::span<const IEarthOrientationProvider::TableEntry> entries() const noexcept override
    {
        return m_entries;
    }

private:
    IEarthOrientationProvider::DataInfo m_info;
    std::vector<IEarthOrientationProvider::TableEntry> m_entries;
};

[[nodiscard]] IEarthOrientationProvider::DataInfo
earthOrientationDataInfo(const EphemerisTableDataStatus status = EphemerisTableDataStatus::Available)
{
    IEarthOrientationProvider::DataInfo info;
    info.version = "test-eop";
    info.provenance = "unit test";
    info.status = status;
    return info;
}

[[nodiscard]] std::vector<IEarthOrientationProvider::TableEntry> fixedEarthOrientationEntries()
{
    return {
        {.effectiveUtcEpoch = epochForDate(2026, 4, 1), .ut1MinusUtcSeconds = 0.10},
        {.effectiveUtcEpoch = epochForDate(2026, 4, 11), .ut1MinusUtcSeconds = 0.30},
    };
}

[[nodiscard]] std::shared_ptr<const IEarthOrientationProvider> earthOrientationProvider(
    IEarthOrientationProvider::DataInfo info = earthOrientationDataInfo(),
    std::vector<IEarthOrientationProvider::TableEntry> entries = fixedEarthOrientationEntries()
)
{
    return std::make_shared<FixedEarthOrientationProvider>(std::move(info), std::move(entries));
}

class FixedDeltaTProvider final : public IDeltaTProvider {
public:
    explicit FixedDeltaTProvider(const double deltaTSeconds) : m_deltaTSeconds(deltaTSeconds)
    {
        m_dataInfo.version = "test-delta-t";
        m_dataInfo.provenance = "unit test";
        m_dataInfo.status = EphemerisTableDataStatus::Available;
    }

    [[nodiscard]] const IDeltaTProvider::DataInfo& dataInfo() const noexcept override
    {
        return m_dataInfo;
    }

    [[nodiscard]] std::span<const IDeltaTProvider::TableEntry> entries() const noexcept override
    {
        return {};
    }

    [[nodiscard]] IDeltaTProvider::Estimate deltaTSeconds(const AstronomicalEpoch&) const override
    {
        return {
            .status = IDeltaTProvider::EstimateStatus::Available,
            .deltaTSeconds = m_deltaTSeconds,
            .diagnosticText = "Delta T estimate resolved.",
            .provenance = "unit test",
        };
    }

private:
    IDeltaTProvider::DataInfo m_dataInfo;
    double m_deltaTSeconds = 0.0;
};

[[nodiscard]] std::shared_ptr<const IDeltaTProvider> deltaTProvider(const double deltaTSeconds)
{
    return std::make_shared<FixedDeltaTProvider>(deltaTSeconds);
}

[[nodiscard]] TimeScaleServiceOptions degradedLeapSecondOptions()
{
    TimeScaleServiceOptions options;
    options.allowDegradedLeapSecondFallback = true;
    options.fallbackTaiMinusUtcSeconds = 42;
    return options;
}

}  // namespace

class TimeScaleOffsetResolverTests final : public QObject {
    Q_OBJECT

private slots:
    void resolvesUtcOffsetsBetweenTransitions();
    void resolvesUtcOffsetAtTransitionInstants();
    void resolvesTaiOffsetAtTransitionInstants();
    void reportsUtcBeforeFirstLeapSecondEntry();
    void reportsTaiBeforeFirstLeapSecondEntry();
    void keepsLastOffsetBeyondFinalEntry();
    void reportsUtcOutsideValidityRange();
    void reportsTaiOutsideValidityRange();
    void resolvesValidityRangeBoundaries();
    void reportsMissingOrMalformedLeapSecondTables();
    void reportsEmptyLeapSecondTables();
    void reportsStaleLeapSecondTables();
    void resolvesExactEarthOrientationRow();
    void resolvesInterpolatedEarthOrientationSample();
    void mapsEarthOrientationDataWarnings();
    void reportsUnusableEarthOrientationWithoutFallback();
    void usesDeltaTFallbackForMissingEarthOrientation();
    void reportsDeltaTUnavailableInFallback();
    void reportsLeapSecondFailureInDeltaTFallback();
    void invertsUt1EpochWithEarthOrientationSamples();
    void invertsUt1EpochWithDeltaTFallback();
    void mergesLookupWarningsIntoConversionResult();
};

void TimeScaleOffsetResolverTests::resolvesUtcOffsetsBetweenTransitions()
{
    const LeapSecondOffsetResolver resolver(leapSecondProvider(), TimeScaleServiceOptions{});

    const LeapSecondOffsetResolver::Result between1972Transitions = resolver.lookupUtcOffset(epochForDate(1972, 3, 1));
    QCOMPARE(between1972Transitions.offsetSeconds.value_or(-1), 10);
    QCOMPARE(
        static_cast<std::uint8_t>(between1972Transitions.status), static_cast<std::uint8_t>(ConversionStatus::Valid)
    );
    QVERIFY(!between1972Transitions.warningCodeMask.hasAny());
    QVERIFY(between1972Transitions.diagnosticText.empty());

    const LeapSecondOffsetResolver::Result afterSecondTransition = resolver.lookupUtcOffset(epochForDate(1972, 8, 15));
    QCOMPARE(afterSecondTransition.offsetSeconds.value_or(-1), 11);
    QCOMPARE(
        static_cast<std::uint8_t>(afterSecondTransition.status), static_cast<std::uint8_t>(ConversionStatus::Valid)
    );

    const LeapSecondOffsetResolver::Result beforeTransition2017 = resolver.lookupUtcOffset(epochForDate(2016, 1, 1));
    QCOMPARE(beforeTransition2017.offsetSeconds.value_or(-1), 36);
    QCOMPARE(
        static_cast<std::uint8_t>(beforeTransition2017.status), static_cast<std::uint8_t>(ConversionStatus::Valid)
    );

    const LeapSecondOffsetResolver::Result afterTransition2017 = resolver.lookupUtcOffset(epochForDate(2018, 1, 1));
    QCOMPARE(afterTransition2017.offsetSeconds.value_or(-1), 37);
    QCOMPARE(static_cast<std::uint8_t>(afterTransition2017.status), static_cast<std::uint8_t>(ConversionStatus::Valid));
}

void TimeScaleOffsetResolverTests::resolvesUtcOffsetAtTransitionInstants()
{
    const LeapSecondOffsetResolver resolver(leapSecondProvider(), TimeScaleServiceOptions{});

    // The transition instant already belongs to the new offset, while the
    // preceding second still uses the previous one.
    const LeapSecondOffsetResolver::Result atTransition1972 = resolver.lookupUtcOffset(epochForDate(1972, 7, 1));
    QCOMPARE(atTransition1972.offsetSeconds.value_or(-1), 11);
    QCOMPARE(static_cast<std::uint8_t>(atTransition1972.status), static_cast<std::uint8_t>(ConversionStatus::Valid));
    QVERIFY(!atTransition1972.warningCodeMask.hasAny());

    const LeapSecondOffsetResolver::Result beforeTransition1972 =
        resolver.lookupUtcOffset(epochForDateTime(1972, 6, 30, 23, 59, 59));
    QCOMPARE(beforeTransition1972.offsetSeconds.value_or(-1), 10);
    QCOMPARE(
        static_cast<std::uint8_t>(beforeTransition1972.status), static_cast<std::uint8_t>(ConversionStatus::Valid)
    );

    const LeapSecondOffsetResolver::Result atTransition2017 = resolver.lookupUtcOffset(epochForDate(2017, 1, 1));
    QCOMPARE(atTransition2017.offsetSeconds.value_or(-1), 37);
    QCOMPARE(static_cast<std::uint8_t>(atTransition2017.status), static_cast<std::uint8_t>(ConversionStatus::Valid));

    const LeapSecondOffsetResolver::Result beforeTransition2017 =
        resolver.lookupUtcOffset(epochForDateTime(2016, 12, 31, 23, 59, 59));
    QCOMPARE(beforeTransition2017.offsetSeconds.value_or(-1), 36);
    QCOMPARE(
        static_cast<std::uint8_t>(beforeTransition2017.status), static_cast<std::uint8_t>(ConversionStatus::Valid)
    );
}

void TimeScaleOffsetResolverTests::resolvesTaiOffsetAtTransitionInstants()
{
    const LeapSecondOffsetResolver resolver(leapSecondProvider(), TimeScaleServiceOptions{});
    const AstronomicalEpoch transition1972Tai = taiEpochFor(epochForDate(1972, 7, 1), 11.0);
    const AstronomicalEpoch transition2017Tai = taiEpochFor(epochForDate(2017, 1, 1), 37.0);

    const LeapSecondOffsetResolver::Result atTransition1972 = resolver.lookupTaiOffset(transition1972Tai);
    QCOMPARE(atTransition1972.offsetSeconds.value_or(-1), 11);
    QCOMPARE(static_cast<std::uint8_t>(atTransition1972.status), static_cast<std::uint8_t>(ConversionStatus::Valid));
    QVERIFY(!atTransition1972.warningCodeMask.hasAny());

    const LeapSecondOffsetResolver::Result beforeTransition1972 =
        resolver.lookupTaiOffset(transition1972Tai.addSeconds(-1.0, TimeScale::Tai));
    QCOMPARE(beforeTransition1972.offsetSeconds.value_or(-1), 10);

    const LeapSecondOffsetResolver::Result atTransition2017 = resolver.lookupTaiOffset(transition2017Tai);
    QCOMPARE(atTransition2017.offsetSeconds.value_or(-1), 37);
    QCOMPARE(static_cast<std::uint8_t>(atTransition2017.status), static_cast<std::uint8_t>(ConversionStatus::Valid));

    const LeapSecondOffsetResolver::Result beforeTransition2017 =
        resolver.lookupTaiOffset(transition2017Tai.addSeconds(-1.0, TimeScale::Tai));
    QCOMPARE(beforeTransition2017.offsetSeconds.value_or(-1), 36);
}

void TimeScaleOffsetResolverTests::reportsUtcBeforeFirstLeapSecondEntry()
{
    const LeapSecondOffsetResolver resolver(leapSecondProvider(), TimeScaleServiceOptions{});
    const LeapSecondOffsetResolver::Result result =
        resolver.lookupUtcOffset(epochForDateTime(1971, 12, 31, 23, 59, 59));

    QVERIFY(!result.offsetSeconds.has_value());
    QCOMPARE(static_cast<std::uint8_t>(result.status), static_cast<std::uint8_t>(ConversionStatus::Failed));
    QVERIFY(hasWarning(result, TimeScaleConversionWarningCode::Type::EpochOutsideLeapSecondTable));
    QVERIFY(!hasWarning(result, TimeScaleConversionWarningCode::Type::LeapSecondFallbackApplied));
    QCOMPARE(
        QString::fromStdString(result.diagnosticText),
        QStringLiteral("UTC epoch is before the first leap-second table entry.")
    );

    const LeapSecondOffsetResolver degradingResolver(leapSecondProvider(), degradedLeapSecondOptions());
    const LeapSecondOffsetResolver::Result degraded =
        degradingResolver.lookupUtcOffset(epochForDateTime(1971, 12, 31, 23, 59, 59));

    QCOMPARE(degraded.offsetSeconds.value_or(-1), 42);
    QCOMPARE(static_cast<std::uint8_t>(degraded.status), static_cast<std::uint8_t>(ConversionStatus::Degraded));
    QVERIFY(hasWarning(degraded, TimeScaleConversionWarningCode::Type::EpochOutsideLeapSecondTable));
    QVERIFY(hasWarning(degraded, TimeScaleConversionWarningCode::Type::LeapSecondFallbackApplied));
    QCOMPARE(
        QString::fromStdString(degraded.diagnosticText),
        QStringLiteral("Time-scale conversion used a degraded leap-second fallback.")
    );
}

void TimeScaleOffsetResolverTests::reportsTaiBeforeFirstLeapSecondEntry()
{
    const AstronomicalEpoch taiEpoch = taiEpochFor(epochForDate(1971, 12, 31), 0.0);

    const LeapSecondOffsetResolver resolver(leapSecondProvider(), TimeScaleServiceOptions{});
    const LeapSecondOffsetResolver::Result result = resolver.lookupTaiOffset(taiEpoch);

    QVERIFY(!result.offsetSeconds.has_value());
    QCOMPARE(static_cast<std::uint8_t>(result.status), static_cast<std::uint8_t>(ConversionStatus::Failed));
    QVERIFY(hasWarning(result, TimeScaleConversionWarningCode::Type::EpochOutsideLeapSecondTable));
    QCOMPARE(
        QString::fromStdString(result.diagnosticText),
        QStringLiteral("TAI epoch is before the first leap-second table entry.")
    );

    const LeapSecondOffsetResolver degradingResolver(leapSecondProvider(), degradedLeapSecondOptions());
    const LeapSecondOffsetResolver::Result degraded = degradingResolver.lookupTaiOffset(taiEpoch);

    QCOMPARE(degraded.offsetSeconds.value_or(-1), 42);
    QCOMPARE(static_cast<std::uint8_t>(degraded.status), static_cast<std::uint8_t>(ConversionStatus::Degraded));
    QVERIFY(hasWarning(degraded, TimeScaleConversionWarningCode::Type::LeapSecondFallbackApplied));
}

void TimeScaleOffsetResolverTests::keepsLastOffsetBeyondFinalEntry()
{
    // Without validity-range metadata the last table row extends forward.
    const LeapSecondOffsetResolver resolver(leapSecondProvider(), TimeScaleServiceOptions{});

    const LeapSecondOffsetResolver::Result utc = resolver.lookupUtcOffset(epochForDate(2030, 1, 1));
    QCOMPARE(utc.offsetSeconds.value_or(-1), 37);
    QCOMPARE(static_cast<std::uint8_t>(utc.status), static_cast<std::uint8_t>(ConversionStatus::Valid));
    QVERIFY(!utc.warningCodeMask.hasAny());

    const LeapSecondOffsetResolver::Result tai = resolver.lookupTaiOffset(taiEpochFor(epochForDate(2030, 1, 1), 0.0));
    QCOMPARE(tai.offsetSeconds.value_or(-1), 37);
    QCOMPARE(static_cast<std::uint8_t>(tai.status), static_cast<std::uint8_t>(ConversionStatus::Valid));
}

void TimeScaleOffsetResolverTests::reportsUtcOutsideValidityRange()
{
    const LeapSecondOffsetResolver resolver(
        leapSecondProvider(leapSecondTableInfo(EphemerisTableDataStatus::Available, leapSecondValidityRange())),
        TimeScaleServiceOptions{}
    );

    const LeapSecondOffsetResolver::Result afterRange = resolver.lookupUtcOffset(epochForDate(2030, 1, 1));
    QVERIFY(!afterRange.offsetSeconds.has_value());
    QCOMPARE(static_cast<std::uint8_t>(afterRange.status), static_cast<std::uint8_t>(ConversionStatus::Failed));
    QVERIFY(hasWarning(afterRange, TimeScaleConversionWarningCode::Type::EpochOutsideLeapSecondTable));
    QCOMPARE(
        QString::fromStdString(afterRange.diagnosticText),
        QStringLiteral("UTC epoch is outside the leap-second table validity range.")
    );

    const LeapSecondOffsetResolver::Result beforeRange = resolver.lookupUtcOffset(epochForDate(1971, 6, 1));
    QVERIFY(!beforeRange.offsetSeconds.has_value());
    QCOMPARE(static_cast<std::uint8_t>(beforeRange.status), static_cast<std::uint8_t>(ConversionStatus::Failed));
    QVERIFY(hasWarning(beforeRange, TimeScaleConversionWarningCode::Type::EpochOutsideLeapSecondTable));

    const LeapSecondOffsetResolver degradingResolver(
        leapSecondProvider(leapSecondTableInfo(EphemerisTableDataStatus::Available, leapSecondValidityRange())),
        degradedLeapSecondOptions()
    );

    const LeapSecondOffsetResolver::Result degradedAfterRange =
        degradingResolver.lookupUtcOffset(epochForDate(2030, 1, 1));
    QCOMPARE(degradedAfterRange.offsetSeconds.value_or(-1), 37);
    QCOMPARE(
        static_cast<std::uint8_t>(degradedAfterRange.status), static_cast<std::uint8_t>(ConversionStatus::Degraded)
    );
    QVERIFY(hasWarning(degradedAfterRange, TimeScaleConversionWarningCode::Type::EpochOutsideLeapSecondTable));
    QVERIFY(hasWarning(degradedAfterRange, TimeScaleConversionWarningCode::Type::LeapSecondFallbackApplied));
    QCOMPARE(
        QString::fromStdString(degradedAfterRange.diagnosticText),
        QStringLiteral("UTC epoch used a degraded leap-second range fallback.")
    );

    // Before the first entry there is no table offset, so the configured
    // fallback supplies the degraded value.
    const LeapSecondOffsetResolver::Result degradedBeforeRange =
        degradingResolver.lookupUtcOffset(epochForDate(1971, 6, 1));
    QCOMPARE(degradedBeforeRange.offsetSeconds.value_or(-1), 42);
    QCOMPARE(
        static_cast<std::uint8_t>(degradedBeforeRange.status), static_cast<std::uint8_t>(ConversionStatus::Degraded)
    );
    QVERIFY(hasWarning(degradedBeforeRange, TimeScaleConversionWarningCode::Type::LeapSecondFallbackApplied));
}

void TimeScaleOffsetResolverTests::reportsTaiOutsideValidityRange()
{
    const LeapSecondOffsetResolver resolver(
        leapSecondProvider(leapSecondTableInfo(EphemerisTableDataStatus::Available, leapSecondValidityRange())),
        TimeScaleServiceOptions{}
    );

    const LeapSecondOffsetResolver::Result afterRange =
        resolver.lookupTaiOffset(taiEpochFor(epochForDate(2030, 1, 1), 0.0));
    QVERIFY(!afterRange.offsetSeconds.has_value());
    QCOMPARE(static_cast<std::uint8_t>(afterRange.status), static_cast<std::uint8_t>(ConversionStatus::Failed));
    QVERIFY(hasWarning(afterRange, TimeScaleConversionWarningCode::Type::EpochOutsideLeapSecondTable));
    QCOMPARE(
        QString::fromStdString(afterRange.diagnosticText),
        QStringLiteral("TAI epoch is outside the leap-second table validity range.")
    );

    const LeapSecondOffsetResolver degradingResolver(
        leapSecondProvider(leapSecondTableInfo(EphemerisTableDataStatus::Available, leapSecondValidityRange())),
        degradedLeapSecondOptions()
    );
    const LeapSecondOffsetResolver::Result degradedAfterRange =
        degradingResolver.lookupTaiOffset(taiEpochFor(epochForDate(2030, 1, 1), 0.0));

    QCOMPARE(degradedAfterRange.offsetSeconds.value_or(-1), 37);
    QCOMPARE(
        static_cast<std::uint8_t>(degradedAfterRange.status), static_cast<std::uint8_t>(ConversionStatus::Degraded)
    );
    QVERIFY(hasWarning(degradedAfterRange, TimeScaleConversionWarningCode::Type::EpochOutsideLeapSecondTable));
    QVERIFY(hasWarning(degradedAfterRange, TimeScaleConversionWarningCode::Type::LeapSecondFallbackApplied));
    QCOMPARE(
        QString::fromStdString(degradedAfterRange.diagnosticText),
        QStringLiteral("TAI epoch used a degraded leap-second range fallback.")
    );
}

void TimeScaleOffsetResolverTests::resolvesValidityRangeBoundaries()
{
    const LeapSecondOffsetResolver resolver(
        leapSecondProvider(leapSecondTableInfo(EphemerisTableDataStatus::Available, leapSecondValidityRange())),
        TimeScaleServiceOptions{}
    );

    const LeapSecondOffsetResolver::Result atUtcStart = resolver.lookupUtcOffset(epochForDate(1972, 1, 1));
    QCOMPARE(atUtcStart.offsetSeconds.value_or(-1), 10);
    QCOMPARE(static_cast<std::uint8_t>(atUtcStart.status), static_cast<std::uint8_t>(ConversionStatus::Valid));

    const LeapSecondOffsetResolver::Result atUtcEnd = resolver.lookupUtcOffset(epochForDate(2027, 1, 1));
    QCOMPARE(atUtcEnd.offsetSeconds.value_or(-1), 37);
    QCOMPARE(static_cast<std::uint8_t>(atUtcEnd.status), static_cast<std::uint8_t>(ConversionStatus::Valid));

    const LeapSecondOffsetResolver::Result afterUtcEnd =
        resolver.lookupUtcOffset(epochForDateTime(2027, 1, 1, 0, 0, 1));
    QVERIFY(!afterUtcEnd.offsetSeconds.has_value());
    QCOMPARE(static_cast<std::uint8_t>(afterUtcEnd.status), static_cast<std::uint8_t>(ConversionStatus::Failed));

    const AstronomicalEpoch startTai = taiEpochFor(epochForDate(1972, 1, 1), 10.0);
    const LeapSecondOffsetResolver::Result atTaiStart = resolver.lookupTaiOffset(startTai);
    QCOMPARE(atTaiStart.offsetSeconds.value_or(-1), 10);
    QCOMPARE(static_cast<std::uint8_t>(atTaiStart.status), static_cast<std::uint8_t>(ConversionStatus::Valid));

    const LeapSecondOffsetResolver::Result beforeTaiStart =
        resolver.lookupTaiOffset(startTai.addSeconds(-1.0, TimeScale::Tai));
    QVERIFY(!beforeTaiStart.offsetSeconds.has_value());
    QCOMPARE(static_cast<std::uint8_t>(beforeTaiStart.status), static_cast<std::uint8_t>(ConversionStatus::Failed));

    const AstronomicalEpoch endTai = taiEpochFor(epochForDate(2027, 1, 1), 37.0);
    const LeapSecondOffsetResolver::Result atTaiEnd = resolver.lookupTaiOffset(endTai);
    QCOMPARE(atTaiEnd.offsetSeconds.value_or(-1), 37);
    QCOMPARE(static_cast<std::uint8_t>(atTaiEnd.status), static_cast<std::uint8_t>(ConversionStatus::Valid));

    const LeapSecondOffsetResolver::Result afterTaiEnd =
        resolver.lookupTaiOffset(endTai.addSeconds(1.0, TimeScale::Tai));
    QVERIFY(!afterTaiEnd.offsetSeconds.has_value());
    QCOMPARE(static_cast<std::uint8_t>(afterTaiEnd.status), static_cast<std::uint8_t>(ConversionStatus::Failed));
}

void TimeScaleOffsetResolverTests::reportsMissingOrMalformedLeapSecondTables()
{
    const LeapSecondOffsetResolver withoutProvider(nullptr, TimeScaleServiceOptions{});
    const AstronomicalEpoch utcEpoch = epochForDate(2026, 1, 1);

    const LeapSecondOffsetResolver::Result missingUtc = withoutProvider.lookupUtcOffset(utcEpoch);
    QVERIFY(!missingUtc.offsetSeconds.has_value());
    QCOMPARE(static_cast<std::uint8_t>(missingUtc.status), static_cast<std::uint8_t>(ConversionStatus::Failed));
    QVERIFY(hasWarning(missingUtc, TimeScaleConversionWarningCode::Type::LeapSecondTableMissing));
    QCOMPARE(
        QString::fromStdString(missingUtc.diagnosticText),
        QStringLiteral("Leap-second table is unavailable for UTC conversion.")
    );

    const LeapSecondOffsetResolver::Result missingTai = withoutProvider.lookupTaiOffset(taiEpochFor(utcEpoch, 0.0));
    QVERIFY(!missingTai.offsetSeconds.has_value());
    QVERIFY(hasWarning(missingTai, TimeScaleConversionWarningCode::Type::LeapSecondTableMissing));
    QCOMPARE(
        QString::fromStdString(missingTai.diagnosticText),
        QStringLiteral("Leap-second table is unavailable for TAI conversion.")
    );

    const std::shared_ptr<const ILeapSecondProvider> malformed =
        leapSecondProvider(leapSecondTableInfo(EphemerisTableDataStatus::Malformed, leapSecondValidityRange()));
    const LeapSecondOffsetResolver malformedResolver(malformed, TimeScaleServiceOptions{});
    const LeapSecondOffsetResolver::Result malformedUtc = malformedResolver.lookupUtcOffset(utcEpoch);
    QVERIFY(!malformedUtc.offsetSeconds.has_value());
    QCOMPARE(static_cast<std::uint8_t>(malformedUtc.status), static_cast<std::uint8_t>(ConversionStatus::Failed));
    QVERIFY(hasWarning(malformedUtc, TimeScaleConversionWarningCode::Type::LeapSecondTableMissing));

    const LeapSecondOffsetResolver degradingResolver(nullptr, degradedLeapSecondOptions());
    const LeapSecondOffsetResolver::Result degraded = degradingResolver.lookupUtcOffset(utcEpoch);
    QCOMPARE(degraded.offsetSeconds.value_or(-1), 42);
    QCOMPARE(static_cast<std::uint8_t>(degraded.status), static_cast<std::uint8_t>(ConversionStatus::Degraded));
    QVERIFY(hasWarning(degraded, TimeScaleConversionWarningCode::Type::LeapSecondTableMissing));
    QVERIFY(hasWarning(degraded, TimeScaleConversionWarningCode::Type::LeapSecondFallbackApplied));
    QCOMPARE(
        QString::fromStdString(degraded.diagnosticText),
        QStringLiteral("Time-scale conversion used a degraded leap-second fallback.")
    );
}

void TimeScaleOffsetResolverTests::reportsEmptyLeapSecondTables()
{
    const LeapSecondOffsetResolver resolver(leapSecondProvider(leapSecondTableInfo(), {}), TimeScaleServiceOptions{});
    const AstronomicalEpoch utcEpoch = epochForDate(2026, 1, 1);

    const LeapSecondOffsetResolver::Result emptyUtc = resolver.lookupUtcOffset(utcEpoch);
    QVERIFY(!emptyUtc.offsetSeconds.has_value());
    QCOMPARE(static_cast<std::uint8_t>(emptyUtc.status), static_cast<std::uint8_t>(ConversionStatus::Failed));
    QVERIFY(hasWarning(emptyUtc, TimeScaleConversionWarningCode::Type::EpochOutsideLeapSecondTable));
    QCOMPARE(
        QString::fromStdString(emptyUtc.diagnosticText),
        QStringLiteral("UTC epoch is before the first leap-second table entry.")
    );

    const LeapSecondOffsetResolver::Result emptyTai = resolver.lookupTaiOffset(taiEpochFor(utcEpoch, 0.0));
    QVERIFY(!emptyTai.offsetSeconds.has_value());
    QCOMPARE(static_cast<std::uint8_t>(emptyTai.status), static_cast<std::uint8_t>(ConversionStatus::Failed));
    QCOMPARE(
        QString::fromStdString(emptyTai.diagnosticText),
        QStringLiteral("TAI epoch is before the first leap-second table entry.")
    );

    const LeapSecondOffsetResolver degradingResolver(
        leapSecondProvider(leapSecondTableInfo(), {}), degradedLeapSecondOptions()
    );
    const LeapSecondOffsetResolver::Result degraded = degradingResolver.lookupUtcOffset(utcEpoch);
    QCOMPARE(degraded.offsetSeconds.value_or(-1), 42);
    QCOMPARE(static_cast<std::uint8_t>(degraded.status), static_cast<std::uint8_t>(ConversionStatus::Degraded));
    QVERIFY(hasWarning(degraded, TimeScaleConversionWarningCode::Type::EpochOutsideLeapSecondTable));
    QVERIFY(hasWarning(degraded, TimeScaleConversionWarningCode::Type::LeapSecondFallbackApplied));
}

void TimeScaleOffsetResolverTests::reportsStaleLeapSecondTables()
{
    const LeapSecondOffsetResolver resolver(
        leapSecondProvider(leapSecondTableInfo(EphemerisTableDataStatus::Stale)), TimeScaleServiceOptions{}
    );
    const AstronomicalEpoch utcEpoch = epochForDate(2018, 1, 1);

    const LeapSecondOffsetResolver::Result staleUtc = resolver.lookupUtcOffset(utcEpoch);
    QCOMPARE(staleUtc.offsetSeconds.value_or(-1), 37);
    QCOMPARE(static_cast<std::uint8_t>(staleUtc.status), static_cast<std::uint8_t>(ConversionStatus::Degraded));
    QVERIFY(hasWarning(staleUtc, TimeScaleConversionWarningCode::Type::LeapSecondTableStale));
    QCOMPARE(
        QString::fromStdString(staleUtc.diagnosticText),
        QStringLiteral("Leap-second table is stale for UTC conversion.")
    );

    const LeapSecondOffsetResolver::Result staleTai = resolver.lookupTaiOffset(taiEpochFor(utcEpoch, 0.0));
    QCOMPARE(staleTai.offsetSeconds.value_or(-1), 37);
    QCOMPARE(static_cast<std::uint8_t>(staleTai.status), static_cast<std::uint8_t>(ConversionStatus::Degraded));
    QVERIFY(hasWarning(staleTai, TimeScaleConversionWarningCode::Type::LeapSecondTableStale));
    QCOMPARE(
        QString::fromStdString(staleTai.diagnosticText),
        QStringLiteral("Leap-second table is stale for TAI conversion.")
    );
}

void TimeScaleOffsetResolverTests::resolvesExactEarthOrientationRow()
{
    const Ut1OffsetResolver resolver(
        leapSecondProvider(), earthOrientationProvider(), nullptr, TimeScaleServiceOptions{}
    );
    const Ut1OffsetResolver::Result lookup = resolver.lookupUtcToUt1Offset(epochForDate(2026, 4, 1));

    QVERIFY(lookup.isSuccess());
    QVERIFY(lookup.ut1MinusUtcSeconds.has_value());
    QCOMPARE(*lookup.ut1MinusUtcSeconds, 0.10);
    QCOMPARE(static_cast<std::uint8_t>(lookup.status), static_cast<std::uint8_t>(ConversionStatus::Valid));
    QVERIFY(!lookup.warningCodeMask.hasAny());
    QCOMPARE(QString::fromStdString(lookup.diagnosticText), QStringLiteral("Earth-orientation sample resolved."));
}

void TimeScaleOffsetResolverTests::resolvesInterpolatedEarthOrientationSample()
{
    const Ut1OffsetResolver resolver(
        leapSecondProvider(), earthOrientationProvider(), nullptr, TimeScaleServiceOptions{}
    );
    const Ut1OffsetResolver::Result lookup = resolver.lookupUtcToUt1Offset(epochForDate(2026, 4, 6));

    QVERIFY(lookup.isSuccess());
    QVERIFY(lookup.ut1MinusUtcSeconds.has_value());
    QVERIFY(std::abs(*lookup.ut1MinusUtcSeconds - 0.20) < 1.0e-12);
    QCOMPARE(static_cast<std::uint8_t>(lookup.status), static_cast<std::uint8_t>(ConversionStatus::Valid));
    QCOMPARE(QString::fromStdString(lookup.diagnosticText), QStringLiteral("Earth-orientation sample interpolated."));
}

void TimeScaleOffsetResolverTests::mapsEarthOrientationDataWarnings()
{
    const Ut1OffsetResolver staleResolver(
        leapSecondProvider(),
        earthOrientationProvider(earthOrientationDataInfo(EphemerisTableDataStatus::Stale)),
        nullptr,
        TimeScaleServiceOptions{}
    );
    const Ut1OffsetResolver::Result stale = staleResolver.lookupUtcToUt1Offset(epochForDate(2026, 4, 1));

    QVERIFY(stale.isSuccess());
    QCOMPARE(static_cast<std::uint8_t>(stale.status), static_cast<std::uint8_t>(ConversionStatus::Degraded));
    QVERIFY(hasWarning(stale, TimeScaleConversionWarningCode::Type::EarthOrientationDataStale));

    const auto predictedEntries = std::vector<IEarthOrientationProvider::TableEntry>{
        {.effectiveUtcEpoch = epochForDate(2026, 4, 1), .ut1MinusUtcSeconds = 0.10, .predicted = true},
        {.effectiveUtcEpoch = epochForDate(2026, 4, 11), .ut1MinusUtcSeconds = 0.30, .predicted = true},
    };
    const Ut1OffsetResolver predictedResolver(
        leapSecondProvider(),
        earthOrientationProvider(earthOrientationDataInfo(), predictedEntries),
        nullptr,
        TimeScaleServiceOptions{}
    );
    const Ut1OffsetResolver::Result predicted = predictedResolver.lookupUtcToUt1Offset(epochForDate(2026, 4, 1));

    QVERIFY(predicted.isSuccess());
    QCOMPARE(static_cast<std::uint8_t>(predicted.status), static_cast<std::uint8_t>(ConversionStatus::Degraded));
    QVERIFY(hasWarning(predicted, TimeScaleConversionWarningCode::Type::EarthOrientationDataPredicted));

    AstronomicalEpoch taiEpoch = epochForDate(2026, 4, 1);
    taiEpoch.timeScale = TimeScale::Tai;
    const Ut1OffsetResolver plainResolver(
        leapSecondProvider(), earthOrientationProvider(), nullptr, TimeScaleServiceOptions{}
    );
    const Ut1OffsetResolver::Result invalid = plainResolver.lookupUtcToUt1Offset(taiEpoch);

    QVERIFY(!invalid.isSuccess());
    QCOMPARE(static_cast<std::uint8_t>(invalid.status), static_cast<std::uint8_t>(ConversionStatus::Failed));
    QVERIFY(hasWarning(invalid, TimeScaleConversionWarningCode::Type::InvalidInput));
    QCOMPARE(
        QString::fromStdString(invalid.diagnosticText),
        QStringLiteral("Earth-orientation sampling requires a finite UTC epoch.")
    );
}

void TimeScaleOffsetResolverTests::reportsUnusableEarthOrientationWithoutFallback()
{
    const Ut1OffsetResolver resolver(leapSecondProvider(), nullptr, deltaTProvider(69.2), TimeScaleServiceOptions{});
    const Ut1OffsetResolver::Result missing = resolver.lookupUtcToUt1Offset(epochForDate(2026, 4, 1));

    QVERIFY(!missing.isSuccess());
    QVERIFY(!missing.ut1MinusUtcSeconds.has_value());
    QCOMPARE(static_cast<std::uint8_t>(missing.status), static_cast<std::uint8_t>(ConversionStatus::Failed));
    QVERIFY(hasWarning(missing, TimeScaleConversionWarningCode::Type::EarthOrientationDataMissing));
    QVERIFY(!hasWarning(missing, TimeScaleConversionWarningCode::Type::DeltaTFallbackApplied));
    QCOMPARE(
        QString::fromStdString(missing.diagnosticText),
        QStringLiteral("Earth-orientation data is unavailable for the requested epoch.")
    );

    const Ut1OffsetResolver outOfRangeResolver(
        leapSecondProvider(), earthOrientationProvider(), nullptr, TimeScaleServiceOptions{}
    );
    const Ut1OffsetResolver::Result outOfRange = outOfRangeResolver.lookupUtcToUt1Offset(epochForDate(2026, 6, 1));

    QVERIFY(!outOfRange.isSuccess());
    QVERIFY(hasWarning(outOfRange, TimeScaleConversionWarningCode::Type::EpochOutsideEarthOrientationData));
    QCOMPARE(
        QString::fromStdString(outOfRange.diagnosticText),
        QStringLiteral("Requested epoch is after the last Earth-orientation row.")
    );

    const std::shared_ptr<const IEarthOrientationProvider> malformed =
        earthOrientationProvider(earthOrientationDataInfo(EphemerisTableDataStatus::Malformed));
    const Ut1OffsetResolver malformedResolver(leapSecondProvider(), malformed, nullptr, TimeScaleServiceOptions{});
    const Ut1OffsetResolver::Result malformedLookup = malformedResolver.lookupUtcToUt1Offset(epochForDate(2026, 4, 1));

    QVERIFY(!malformedLookup.isSuccess());
    QVERIFY(hasWarning(malformedLookup, TimeScaleConversionWarningCode::Type::EarthOrientationDataMissing));
}

void TimeScaleOffsetResolverTests::usesDeltaTFallbackForMissingEarthOrientation()
{
    TimeScaleServiceOptions options;
    options.allowUt1DeltaTFallback = true;

    const Ut1OffsetResolver resolver(leapSecondProvider(), nullptr, deltaTProvider(69.2), options);
    const Ut1OffsetResolver::Result lookup = resolver.lookupUtcToUt1Offset(epochForDate(2026, 4, 1));

    QVERIFY(lookup.isSuccess());
    QCOMPARE(static_cast<std::uint8_t>(lookup.status), static_cast<std::uint8_t>(ConversionStatus::Degraded));

    // ut1 - utc = (tai - utc) + (tt - tai) - (tt - ut1)
    const double expectedSeconds = 37.0 + TimeConstants::kTtMinusTaiSeconds - 69.2;
    QVERIFY(lookup.ut1MinusUtcSeconds.has_value());
    QVERIFY(std::abs(*lookup.ut1MinusUtcSeconds - expectedSeconds) < 1.0e-9);
    QVERIFY(hasWarning(lookup, TimeScaleConversionWarningCode::Type::EarthOrientationDataMissing));
    QVERIFY(hasWarning(lookup, TimeScaleConversionWarningCode::Type::DeltaTFallbackApplied));
    QVERIFY(!hasWarning(lookup, TimeScaleConversionWarningCode::Type::LeapSecondFallbackApplied));
    QCOMPARE(QString::fromStdString(lookup.diagnosticText), QStringLiteral("Delta T estimate resolved."));
}

void TimeScaleOffsetResolverTests::reportsDeltaTUnavailableInFallback()
{
    TimeScaleServiceOptions options;
    options.allowUt1DeltaTFallback = true;

    const Ut1OffsetResolver resolver(leapSecondProvider(), nullptr, nullptr, options);
    const Ut1OffsetResolver::Result lookup = resolver.lookupUtcToUt1Offset(epochForDate(2026, 4, 1));

    QVERIFY(!lookup.isSuccess());
    QVERIFY(!lookup.ut1MinusUtcSeconds.has_value());
    QCOMPARE(static_cast<std::uint8_t>(lookup.status), static_cast<std::uint8_t>(ConversionStatus::Failed));
    QVERIFY(hasWarning(lookup, TimeScaleConversionWarningCode::Type::DeltaTUnavailable));
    QCOMPARE(
        QString::fromStdString(lookup.diagnosticText),
        QStringLiteral("UT1 conversion could not use Delta T fallback because data is unavailable.")
    );
}

void TimeScaleOffsetResolverTests::reportsLeapSecondFailureInDeltaTFallback()
{
    TimeScaleServiceOptions options;
    options.allowUt1DeltaTFallback = true;

    const Ut1OffsetResolver resolver(
        leapSecondProvider(leapSecondTableInfo(), {}), nullptr, deltaTProvider(69.2), options
    );
    const Ut1OffsetResolver::Result lookup = resolver.lookupUtcToUt1Offset(epochForDate(2026, 4, 1));

    QVERIFY(!lookup.isSuccess());
    QVERIFY(!lookup.ut1MinusUtcSeconds.has_value());
    QCOMPARE(static_cast<std::uint8_t>(lookup.status), static_cast<std::uint8_t>(ConversionStatus::Failed));
    QVERIFY(hasWarning(lookup, TimeScaleConversionWarningCode::Type::EpochOutsideLeapSecondTable));
    QCOMPARE(
        QString::fromStdString(lookup.diagnosticText),
        QStringLiteral("UTC epoch is before the first leap-second table entry.")
    );
}

void TimeScaleOffsetResolverTests::invertsUt1EpochWithEarthOrientationSamples()
{
    const Ut1OffsetResolver resolver(
        leapSecondProvider(), earthOrientationProvider(), nullptr, TimeScaleServiceOptions{}
    );
    const AstronomicalEpoch utcEpoch = epochForDate(2026, 4, 6);
    const AstronomicalEpoch ut1Epoch = utcEpoch.addSeconds(0.20, TimeScale::Ut1);

    const TimeScaleConversionResult result = resolver.lookupUtcFromUt1(ut1Epoch);

    QVERIFY(result.isSuccess());
    QCOMPARE(static_cast<std::uint8_t>(result.status), static_cast<std::uint8_t>(ConversionStatus::Valid));
    QCOMPARE(static_cast<std::uint8_t>(result.epoch.timeScale), static_cast<std::uint8_t>(TimeScale::Utc));
    QVERIFY(std::abs(secondsBetween(result.epoch, utcEpoch)) < 1.0e-3);
    QVERIFY(!result.warningCodeMask.hasAny());
    QCOMPARE(QString::fromStdString(result.diagnosticText), QStringLiteral("Earth-orientation sample interpolated."));
}

void TimeScaleOffsetResolverTests::invertsUt1EpochWithDeltaTFallback()
{
    TimeScaleServiceOptions options;
    options.allowUt1DeltaTFallback = true;

    const Ut1OffsetResolver resolver(leapSecondProvider(), nullptr, deltaTProvider(69.2), options);
    const AstronomicalEpoch ut1Epoch = epochForDate(2026, 4, 6).addSeconds(0.0, TimeScale::Ut1);

    const TimeScaleConversionResult result = resolver.lookupUtcFromUt1(ut1Epoch);

    QVERIFY(result.isSuccess());
    QCOMPARE(static_cast<std::uint8_t>(result.status), static_cast<std::uint8_t>(ConversionStatus::Degraded));
    QCOMPARE(static_cast<std::uint8_t>(result.epoch.timeScale), static_cast<std::uint8_t>(TimeScale::Utc));
    QVERIFY(result.hasWarning(TimeScaleConversionWarningCode::Type::EarthOrientationDataMissing));
    QVERIFY(result.hasWarning(TimeScaleConversionWarningCode::Type::DeltaTFallbackApplied));

    // utc = ut1 + deltaT - (tt - tai) - (tai - utc)
    const double expectedOffsetSeconds = 69.2 - TimeConstants::kTtMinusTaiSeconds - 37.0;
    QVERIFY(std::abs(secondsBetween(result.epoch, ut1Epoch) - expectedOffsetSeconds) < 1.0e-3);
    QCOMPARE(QString::fromStdString(result.diagnosticText), QStringLiteral("Delta T estimate resolved."));
}

void TimeScaleOffsetResolverTests::mergesLookupWarningsIntoConversionResult()
{
    TimeScaleConversionResult result = TimeScaleConversionResult::success(epochForDate(2026, 4, 1));
    QCOMPARE(static_cast<std::uint8_t>(result.status), static_cast<std::uint8_t>(ConversionStatus::Valid));
    QVERIFY(!result.warningCodeMask.hasAny());

    Ut1OffsetResolver::Result lookup;
    lookup.ut1MinusUtcSeconds = 0.10;
    lookup.status = ConversionStatus::Degraded;
    lookup.addWarning(TimeScaleConversionWarningCode::Type::EarthOrientationDataPredicted);
    lookup.diagnosticText = "Earth-orientation sample interpolated with degraded metadata.";

    Ut1OffsetResolver::mergeWarnings(result, lookup);

    QCOMPARE(static_cast<std::uint8_t>(result.status), static_cast<std::uint8_t>(ConversionStatus::Degraded));
    QVERIFY(result.isSuccess());
    QVERIFY(result.hasWarning(TimeScaleConversionWarningCode::Type::EarthOrientationDataPredicted));
    QCOMPARE(result.diagnosticText, std::string("Time-scale conversion succeeded."));
}

QTEST_APPLESS_MAIN(TimeScaleOffsetResolverTests)

#include "TimeScaleOffsetResolverTests.moc"
