#include "engine/EarthOrientationSampler.hpp"
#include "engine/EphemerisTimePolicy.hpp"
#include "engine/LeapSecondTimeScaleService.hpp"
#include "engine/TimeScaleServiceOptions.hpp"
#include "engine/Ut1OffsetResolver.hpp"
#include "math/TimeConstants.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/CalendarTime.hpp"
#include "time/CivilDateTime.hpp"
#include "time/TimeScale.hpp"

#include <QtTest/QtTest>

#include <cmath>
#include <cstdint>
#include <memory>
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
using skygate::ephemeris::EarthOrientationSampler;
using skygate::ephemeris::EphemerisTimePolicy;
using skygate::ephemeris::IEarthOrientationProvider;
using skygate::ephemeris::LeapSecondTimeScaleService;
using skygate::ephemeris::TimeScaleConversionResult;
using skygate::ephemeris::TimeScaleServiceOptions;
using skygate::ephemeris::Ut1OffsetResolver;

class StubEarthOrientationProvider final : public IEarthOrientationProvider {
public:
    StubEarthOrientationProvider(
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

[[nodiscard]] AstronomicalEpoch epochForDate(const int year, const int month, const int day)
{
    const auto epoch = CalendarTime::astronomicalEpochFromCivilDateTime(
        CivilDateTime{.astronomicalYear = year, .month = month, .day = day, .timeScale = TimeScale::Utc}
    );
    Q_ASSERT(epoch.has_value());
    return *epoch;
}

[[nodiscard]] IEarthOrientationProvider::DataInfo availableDataInfo()
{
    IEarthOrientationProvider::DataInfo info;
    info.version = "test-eop";
    info.provenance = "unit test";
    info.status = IEarthOrientationProvider::DataStatus::Available;
    return info;
}

[[nodiscard]] IEarthOrientationProvider::TableEntry
tableEntry(const int day, const double ut1MinusUtcSeconds, const bool predicted = false)
{
    return {
        .effectiveUtcEpoch = epochForDate(2026, 4, day),
        .ut1MinusUtcSeconds = ut1MinusUtcSeconds,
        .polarMotionXArcseconds = 0.1,
        .polarMotionYArcseconds = 0.2,
        .predicted = predicted,
    };
}

[[nodiscard]] std::vector<IEarthOrientationProvider::TableEntry> tableEntries()
{
    return {tableEntry(1, 0.10), tableEntry(11, 0.30)};
}

[[nodiscard]] std::shared_ptr<const IEarthOrientationProvider> providerWith(
    IEarthOrientationProvider::DataInfo info,
    std::vector<IEarthOrientationProvider::TableEntry> entries = tableEntries()
)
{
    return std::make_shared<StubEarthOrientationProvider>(std::move(info), std::move(entries));
}

}  // namespace

class EarthOrientationSamplerTests final : public QObject {
    Q_OBJECT

private slots:
    void reportsExactRowOrigin();
    void reportsInterpolatedOrigin();
    void reportsNearestRowFallbacks();
    void reportsZeroFallbackOrigin();
    void derivesDiagnosticTextPerOrigin();
    void reportsStaleMetadataAsDegraded();
    void reportsPredictedMetadataAsDegraded();
    void reportsEstimatedMetadataAsDegraded();
    void keepsPredictedRowsValidWhenDegradationIsOff();
    void keepsFailureTextForRejectedRanges();
    void propagatesOriginTextToUt1Resolution();
    void propagatesOriginTextToTimeScaleConversion();
    void convergesUt1InverseAtIterationCap();
};

void EarthOrientationSamplerTests::reportsExactRowOrigin()
{
    const EarthOrientationSampler::Sample sample =
        EarthOrientationSampler::sample(providerWith(availableDataInfo()), epochForDate(2026, 4, 1));

    QVERIFY(sample.isSuccess());
    QCOMPARE(sample.status, EarthOrientationSampler::Sample::Status::Valid);
    QCOMPARE(sample.origin, EarthOrientationSampler::Sample::Origin::ExactRow);
    QCOMPARE(sample.ut1MinusUtcSeconds, 0.10);
    QCOMPARE(sample.polarMotionXArcseconds, 0.1);
    QCOMPARE(sample.diagnosticText, std::string{"Earth-orientation sample resolved."});
}

void EarthOrientationSamplerTests::reportsInterpolatedOrigin()
{
    const EarthOrientationSampler::Sample sample =
        EarthOrientationSampler::sample(providerWith(availableDataInfo()), epochForDate(2026, 4, 6));

    QVERIFY(sample.isSuccess());
    QCOMPARE(sample.status, EarthOrientationSampler::Sample::Status::Valid);
    QCOMPARE(sample.origin, EarthOrientationSampler::Sample::Origin::Interpolated);
    QVERIFY(std::abs(sample.ut1MinusUtcSeconds - 0.20) < 1.0e-12);
    QVERIFY(std::abs(sample.polarMotionXArcseconds - 0.1) < 1.0e-12);
    QVERIFY(std::abs(sample.polarMotionYArcseconds - 0.2) < 1.0e-12);
    QCOMPARE(sample.diagnosticText, std::string{"Earth-orientation sample interpolated."});
}

void EarthOrientationSamplerTests::reportsNearestRowFallbacks()
{
    const std::shared_ptr<const IEarthOrientationProvider> provider = providerWith(availableDataInfo());
    const EarthOrientationSampler::Sample beforeFirst =
        EarthOrientationSampler::sample(provider, epochForDate(2026, 3, 1));
    const EarthOrientationSampler::Sample afterLast =
        EarthOrientationSampler::sample(provider, epochForDate(2026, 5, 1));

    QCOMPARE(beforeFirst.status, EarthOrientationSampler::Sample::Status::Degraded);
    QCOMPARE(afterLast.status, EarthOrientationSampler::Sample::Status::Degraded);
    QCOMPARE(beforeFirst.origin, EarthOrientationSampler::Sample::Origin::NearestRow);
    QCOMPARE(afterLast.origin, EarthOrientationSampler::Sample::Origin::NearestRow);
    QVERIFY(beforeFirst.hasWarning(EarthOrientationSampler::Sample::WarningCode::EpochOutsideRange));
    QVERIFY(afterLast.hasWarning(EarthOrientationSampler::Sample::WarningCode::EpochOutsideRange));
    QCOMPARE(beforeFirst.ut1MinusUtcSeconds, 0.10);
    QCOMPARE(afterLast.ut1MinusUtcSeconds, 0.30);
    QCOMPARE(
        beforeFirst.diagnosticText,
        std::string{"Earth-orientation sample used the first available row as a degraded fallback."}
    );
    QCOMPARE(
        afterLast.diagnosticText,
        std::string{"Earth-orientation sample used the last available row as a degraded fallback."}
    );
}

void EarthOrientationSamplerTests::reportsZeroFallbackOrigin()
{
    EarthOrientationSampler::Options options;
    options.allowMissingDataZeroFallback = true;

    const EarthOrientationSampler::Sample sample =
        EarthOrientationSampler::sample(nullptr, epochForDate(2026, 4, 1), options);

    QVERIFY(sample.isSuccess());
    QCOMPARE(sample.status, EarthOrientationSampler::Sample::Status::Degraded);
    QCOMPARE(sample.origin, EarthOrientationSampler::Sample::Origin::ZeroFallback);
    QCOMPARE(sample.ut1MinusUtcSeconds, 0.0);
    QCOMPARE(sample.polarMotionXArcseconds, 0.0);
    QCOMPARE(sample.polarMotionYArcseconds, 0.0);
    QVERIFY(sample.hasWarning(EarthOrientationSampler::Sample::WarningCode::MissingData));
    QCOMPARE(sample.diagnosticText, std::string{"Earth-orientation data used a degraded zero-value fallback."});
}

void EarthOrientationSamplerTests::derivesDiagnosticTextPerOrigin()
{
    using Origin = EarthOrientationSampler::Sample::Origin;
    using Status = EarthOrientationSampler::Sample::Status;

    QCOMPARE(
        EarthOrientationSampler::Sample::diagnosticTextFor(Origin::ExactRow, Status::Valid, false),
        std::string{"Earth-orientation sample resolved."}
    );
    QCOMPARE(
        EarthOrientationSampler::Sample::diagnosticTextFor(Origin::ExactRow, Status::Degraded, false),
        std::string{"Earth-orientation sample resolved with degraded metadata."}
    );
    QCOMPARE(
        EarthOrientationSampler::Sample::diagnosticTextFor(Origin::Interpolated, Status::Valid, false),
        std::string{"Earth-orientation sample interpolated."}
    );
    QCOMPARE(
        EarthOrientationSampler::Sample::diagnosticTextFor(Origin::Interpolated, Status::Degraded, false),
        std::string{"Earth-orientation sample interpolated with degraded metadata."}
    );
    QCOMPARE(
        EarthOrientationSampler::Sample::diagnosticTextFor(Origin::NearestRow, Status::Degraded, true),
        std::string{"Earth-orientation sample used the first available row as a degraded fallback."}
    );
    QCOMPARE(
        EarthOrientationSampler::Sample::diagnosticTextFor(Origin::NearestRow, Status::Degraded, false),
        std::string{"Earth-orientation sample used the last available row as a degraded fallback."}
    );
    QCOMPARE(
        EarthOrientationSampler::Sample::diagnosticTextFor(Origin::ZeroFallback, Status::Degraded, false),
        std::string{"Earth-orientation data used a degraded zero-value fallback."}
    );
}

void EarthOrientationSamplerTests::reportsStaleMetadataAsDegraded()
{
    IEarthOrientationProvider::DataInfo info = availableDataInfo();
    info.status = IEarthOrientationProvider::DataStatus::Stale;

    const EarthOrientationSampler::Sample sample =
        EarthOrientationSampler::sample(providerWith(std::move(info)), epochForDate(2026, 4, 1));

    QCOMPARE(sample.status, EarthOrientationSampler::Sample::Status::Degraded);
    QCOMPARE(sample.origin, EarthOrientationSampler::Sample::Origin::ExactRow);
    QVERIFY(sample.hasWarning(EarthOrientationSampler::Sample::WarningCode::StaleData));
    QCOMPARE(sample.diagnosticText, std::string{"Earth-orientation sample resolved with degraded metadata."});
}

void EarthOrientationSamplerTests::reportsPredictedMetadataAsDegraded()
{
    const std::shared_ptr<const IEarthOrientationProvider> provider =
        providerWith(availableDataInfo(), {tableEntry(1, 0.10, true), tableEntry(11, 0.30, true)});

    const EarthOrientationSampler::Sample exactRow =
        EarthOrientationSampler::sample(provider, epochForDate(2026, 4, 1));
    QCOMPARE(exactRow.status, EarthOrientationSampler::Sample::Status::Degraded);
    QCOMPARE(exactRow.origin, EarthOrientationSampler::Sample::Origin::ExactRow);
    QVERIFY(exactRow.predicted);
    QVERIFY(exactRow.hasWarning(EarthOrientationSampler::Sample::WarningCode::PredictedData));
    QCOMPARE(exactRow.diagnosticText, std::string{"Earth-orientation sample resolved with degraded metadata."});

    const EarthOrientationSampler::Sample interpolated =
        EarthOrientationSampler::sample(provider, epochForDate(2026, 4, 6));
    QCOMPARE(interpolated.status, EarthOrientationSampler::Sample::Status::Degraded);
    QCOMPARE(interpolated.origin, EarthOrientationSampler::Sample::Origin::Interpolated);
    QVERIFY(interpolated.predicted);
    QVERIFY(interpolated.hasWarning(EarthOrientationSampler::Sample::WarningCode::PredictedData));
    QCOMPARE(interpolated.diagnosticText, std::string{"Earth-orientation sample interpolated with degraded metadata."});
}

void EarthOrientationSamplerTests::reportsEstimatedMetadataAsDegraded()
{
    IEarthOrientationProvider::DataInfo info = availableDataInfo();
    info.status = IEarthOrientationProvider::DataStatus::Estimated;

    const EarthOrientationSampler::Sample sample =
        EarthOrientationSampler::sample(providerWith(std::move(info)), epochForDate(2026, 4, 1));

    QCOMPARE(sample.status, EarthOrientationSampler::Sample::Status::Degraded);
    QCOMPARE(sample.origin, EarthOrientationSampler::Sample::Origin::ExactRow);
    QVERIFY(sample.estimated);
    QVERIFY(sample.hasWarning(EarthOrientationSampler::Sample::WarningCode::EstimatedData));
    QCOMPARE(sample.diagnosticText, std::string{"Earth-orientation sample resolved with degraded metadata."});
}

void EarthOrientationSamplerTests::keepsPredictedRowsValidWhenDegradationIsOff()
{
    EarthOrientationSampler::Options options;
    options.degradePredictedData = false;

    const EarthOrientationSampler::Sample sample = EarthOrientationSampler::sample(
        providerWith(availableDataInfo(), {tableEntry(1, 0.10, true), tableEntry(11, 0.30, true)}),
        epochForDate(2026, 4, 1),
        options
    );

    QCOMPARE(sample.status, EarthOrientationSampler::Sample::Status::Valid);
    QCOMPARE(sample.origin, EarthOrientationSampler::Sample::Origin::ExactRow);
    QVERIFY(sample.predicted);
    QVERIFY(sample.hasWarning(EarthOrientationSampler::Sample::WarningCode::PredictedData));
    QCOMPARE(sample.diagnosticText, std::string{"Earth-orientation sample resolved."});
}

void EarthOrientationSamplerTests::keepsFailureTextForRejectedRanges()
{
    EarthOrientationSampler::Options options;
    options.allowOutOfRangeNearestSampleFallback = false;
    const std::shared_ptr<const IEarthOrientationProvider> provider = providerWith(availableDataInfo());

    const EarthOrientationSampler::Sample beforeFirst =
        EarthOrientationSampler::sample(provider, epochForDate(2026, 3, 1), options);
    QCOMPARE(beforeFirst.status, EarthOrientationSampler::Sample::Status::Failed);
    QCOMPARE(beforeFirst.diagnosticText, std::string{"Requested epoch is before the first Earth-orientation row."});

    const EarthOrientationSampler::Sample afterLast =
        EarthOrientationSampler::sample(provider, epochForDate(2026, 5, 1), options);
    QCOMPARE(afterLast.status, EarthOrientationSampler::Sample::Status::Failed);
    QCOMPARE(afterLast.diagnosticText, std::string{"Requested epoch is after the last Earth-orientation row."});

    const EarthOrientationSampler::Sample missing = EarthOrientationSampler::sample(nullptr, epochForDate(2026, 4, 1));
    QCOMPARE(missing.status, EarthOrientationSampler::Sample::Status::Failed);
    QCOMPARE(missing.diagnosticText, std::string{"Earth-orientation data is unavailable for the requested epoch."});
}

void EarthOrientationSamplerTests::propagatesOriginTextToUt1Resolution()
{
    const std::shared_ptr<const IEarthOrientationProvider> provider = providerWith(availableDataInfo());
    const AstronomicalEpoch epoch = epochForDate(2026, 4, 6);
    const EarthOrientationSampler::Sample sample = EarthOrientationSampler::sample(provider, epoch);
    QCOMPARE(sample.origin, EarthOrientationSampler::Sample::Origin::Interpolated);

    const Ut1OffsetResolver resolver(nullptr, provider, nullptr, TimeScaleServiceOptions{});
    const Ut1OffsetResolver::Result lookup = resolver.lookupUtcToUt1Offset(epoch);

    QVERIFY(lookup.isSuccess());
    QVERIFY(lookup.offsetSeconds.has_value());
    QCOMPARE(lookup.diagnosticText, sample.diagnosticText);
}

void EarthOrientationSamplerTests::propagatesOriginTextToTimeScaleConversion()
{
    const std::shared_ptr<const IEarthOrientationProvider> provider = providerWith(availableDataInfo());
    const AstronomicalEpoch epoch = epochForDate(2026, 4, 1);
    const EarthOrientationSampler::Sample sample = EarthOrientationSampler::sample(provider, epoch);

    const LeapSecondTimeScaleService service(nullptr, TimeScaleServiceOptions{}, provider);
    const TimeScaleConversionResult result = service.convert(epoch, TimeScale::Ut1);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.diagnosticText, sample.diagnosticText);
}

void EarthOrientationSamplerTests::convergesUt1InverseAtIterationCap()
{
    // The table drifts much faster than real Earth-orientation data so that the
    // first inversion round lands well outside the epoch-key tolerance and the
    // solver has to keep refining until its iteration cap.
    const std::shared_ptr<const IEarthOrientationProvider> provider = providerWith(
        availableDataInfo(),
        std::vector<IEarthOrientationProvider::TableEntry>{tableEntry(1, 0.0), tableEntry(11, 8640.0)}
    );
    const AstronomicalEpoch ut1Epoch = epochForDate(2026, 4, 1).addSeconds(43.2, TimeScale::Ut1);
    const AstronomicalEpoch ut1ReadingAsUtc = ut1Epoch.addSeconds(0.0, TimeScale::Utc);

    const EarthOrientationSampler::Sample firstRound = EarthOrientationSampler::sample(provider, ut1ReadingAsUtc);
    QVERIFY(firstRound.isSuccess());
    QVERIFY(
        std::abs(firstRound.ut1MinusUtcSeconds) / TimeConstants::kSecondsPerDay
        > EphemerisTimePolicy::kEpochKeyEqualityToleranceDays
    );

    const Ut1OffsetResolver resolver(nullptr, provider, nullptr, TimeScaleServiceOptions{});
    const TimeScaleConversionResult result = resolver.lookupUtcFromUt1(ut1Epoch);

    QVERIFY(result.isSuccess());
    QCOMPARE(static_cast<std::uint8_t>(result.epoch.timeScale), static_cast<std::uint8_t>(TimeScale::Utc));

    const EarthOrientationSampler::Sample convergedRound = EarthOrientationSampler::sample(provider, result.epoch);
    QVERIFY(convergedRound.isSuccess());
    const AstronomicalEpoch reconstructed = result.epoch.addSeconds(convergedRound.ut1MinusUtcSeconds, TimeScale::Ut1);
    QVERIFY(
        std::abs(reconstructed.sortKey() - ut1Epoch.sortKey()) <= EphemerisTimePolicy::kEpochKeyEqualityToleranceDays
    );
}

QTEST_APPLESS_MAIN(EarthOrientationSamplerTests)

#include "EarthOrientationSamplerTests.moc"
