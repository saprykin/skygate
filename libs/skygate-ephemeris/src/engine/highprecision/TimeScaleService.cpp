#include "TimeScaleService.hpp"
#include "time/CalendarTime.hpp"

#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
#include "engine/highprecision/ErfaAstrometry.hpp"
#endif
#include "math/MathConstants.hpp"
#include "math/TimeConstants.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace skygate::ephemeris {

namespace {

using skygate::core::MathConstants;
using skygate::core::TimeConstants;

struct OffsetLookupResult {
    std::optional<int> offsetSeconds;
    TimeScaleConversionStatus status = TimeScaleConversionStatus::Valid;
    std::uint32_t warningCodeMask = 0U;
    std::string diagnosticText;

    void addWarning(const TimeScaleConversionWarningCode code) noexcept
    {
        warningCodeMask |= timeScaleConversionWarningMask(code);
    }
};

struct Ut1OffsetLookupResult {
    std::optional<double> ut1MinusUtcSeconds;
    TimeScaleConversionStatus status = TimeScaleConversionStatus::Valid;
    std::uint32_t warningCodeMask = 0U;
    std::string diagnosticText;

    void addWarning(const TimeScaleConversionWarningCode code) noexcept
    {
        warningCodeMask |= timeScaleConversionWarningMask(code);
    }

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return status == TimeScaleConversionStatus::Valid || status == TimeScaleConversionStatus::Degraded;
    }
};

[[nodiscard]] AstronomicalEpoch
addSeconds(const AstronomicalEpoch& epoch, const double seconds, const TimeScale targetScale) noexcept
{
    return AstronomicalEpoch{
        .julianDatePart1 = epoch.julianDatePart1,
        .julianDatePart2 = epoch.julianDatePart2 + seconds / TimeConstants::kSecondsPerDay,
        .timeScale = targetScale,
    }
        .normalized();
}

[[nodiscard]] double epochJulianDate(const AstronomicalEpoch& epoch) noexcept
{
    return epoch.julianDatePart1 + epoch.julianDatePart2;
}

[[nodiscard]] double fractionalDay(const AstronomicalEpoch& epoch) noexcept
{
    const double fraction = epochJulianDate(epoch) - std::floor(epochJulianDate(epoch));
    return fraction < 0.0 ? fraction + 1.0 : fraction;
}

[[nodiscard]] double approximateTdbMinusTtSeconds(const AstronomicalEpoch& terrestrialTime) noexcept
{
    const double daysSinceJ2000 = epochJulianDate(terrestrialTime) - TimeConstants::kJulianDateJ2000;
    const double meanAnomalyRadians =
        std::fmod(357.53 + 0.9856003 * daysSinceJ2000, 360.0) * MathConstants::kDegreesToRadians;
    return 0.001657 * std::sin(meanAnomalyRadians) + 0.00001385 * std::sin(2.0 * meanAnomalyRadians);
}

[[nodiscard]] std::optional<double> tdbMinusTtSeconds(const AstronomicalEpoch& terrestrialTime) noexcept
{
#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
    const std::optional<double> erfaResult = skygate::ephemeris::highprecision::ErfaAstrometry::tdbMinusTtSeconds(
        terrestrialTime, fractionalDay(terrestrialTime)
    );
    if (erfaResult.has_value()) {
        return erfaResult;
    }
#endif

    if (!terrestrialTime.isFinite()) {
        return std::nullopt;
    }

    return approximateTdbMinusTtSeconds(terrestrialTime);
}

[[nodiscard]] TimeScaleConversionResult successResult(
    AstronomicalEpoch epoch,
    const TimeScaleConversionStatus status = TimeScaleConversionStatus::Valid,
    const std::uint32_t warningCodeMask = 0U,
    std::string diagnosticText = {}
)
{
    if (diagnosticText.empty()) {
        diagnosticText = status == TimeScaleConversionStatus::Valid ? "Time-scale conversion succeeded."
                                                                    : "Time-scale conversion degraded.";
    }

    return TimeScaleConversionResult{
        .epoch = epoch.normalized(),
        .status = status,
        .warningCodeMask = warningCodeMask,
        .diagnosticText = std::move(diagnosticText),
    };
}

[[nodiscard]] TimeScaleConversionResult
failureResult(const AstronomicalEpoch& epoch, const TimeScale targetScale, std::string diagnosticText)
{
    TimeScaleConversionResult result;
    result.epoch = epoch;
    result.epoch.timeScale = targetScale;
    result.status = TimeScaleConversionStatus::Failed;
    result.diagnosticText = std::move(diagnosticText);
    return result;
}

[[nodiscard]] bool isUtcLeapSecondLabel(const CivilDateTime& dateTime) noexcept
{
    return dateTime.timeScale == TimeScale::Utc && dateTime.hour == 23 && dateTime.minute == 59
           && dateTime.second == 60;
}

[[nodiscard]] bool epochOutsideRange(const AstronomicalEpoch& epoch, const EphemerisDateRange& range) noexcept
{
    const double key = epoch.sortKey();
    return key < range.start.sortKey() || key > range.end.sortKey();
}

[[nodiscard]] bool taiEpochOutsideUtcRange(
    const AstronomicalEpoch& taiEpoch,
    const EphemerisDateRange& utcRange,
    const std::shared_ptr<const ILeapSecondProvider>& provider
) noexcept
{
    const std::optional<int> startOffsetSeconds = provider->taiMinusUtcSeconds(utcRange.start);
    const std::optional<int> endOffsetSeconds = provider->taiMinusUtcSeconds(utcRange.end);
    if (!startOffsetSeconds.has_value() || !endOffsetSeconds.has_value()) {
        return true;
    }

    const double key = taiEpoch.sortKey();
    const AstronomicalEpoch startTaiEpoch =
        addSeconds(utcRange.start, static_cast<double>(*startOffsetSeconds), TimeScale::Tai);
    const AstronomicalEpoch endTaiEpoch =
        addSeconds(utcRange.end, static_cast<double>(*endOffsetSeconds), TimeScale::Tai);
    return key < startTaiEpoch.sortKey() || key > endTaiEpoch.sortKey();
}

[[nodiscard]] OffsetLookupResult fallbackOffset(
    const TimeScaleServiceOptions& options, TimeScaleConversionWarningCode warningCode, std::string diagnosticText
)
{
    OffsetLookupResult result;
    if (!options.allowDegradedLeapSecondFallback) {
        result.status = TimeScaleConversionStatus::Failed;
        result.diagnosticText = std::move(diagnosticText);
        result.addWarning(warningCode);
        return result;
    }

    result.offsetSeconds = options.fallbackTaiMinusUtcSeconds;
    result.status = TimeScaleConversionStatus::Degraded;
    result.addWarning(warningCode);
    result.addWarning(TimeScaleConversionWarningCode::LeapSecondFallbackApplied);
    result.diagnosticText = "Time-scale conversion used a degraded leap-second fallback.";
    return result;
}

[[nodiscard]] OffsetLookupResult lookupUtcOffset(
    const std::shared_ptr<const ILeapSecondProvider>& provider,
    const TimeScaleServiceOptions& options,
    const AstronomicalEpoch& utcEpoch
)
{
    if (provider == nullptr || !provider->tableInfo().isUsable()) {
        return fallbackOffset(
            options,
            TimeScaleConversionWarningCode::LeapSecondTableMissing,
            "Leap-second table is unavailable for UTC conversion."
        );
    }

    OffsetLookupResult result;
    const LeapSecondTableInfo& tableInfo = provider->tableInfo();
    if (tableInfo.status == LeapSecondTableStatus::Stale) {
        result.status = TimeScaleConversionStatus::Degraded;
        result.addWarning(TimeScaleConversionWarningCode::LeapSecondTableStale);
        result.diagnosticText = "Leap-second table is stale for UTC conversion.";
    }

    if (tableInfo.validityRange.has_value() && epochOutsideRange(utcEpoch, *tableInfo.validityRange)) {
        const std::optional<int> tableOffset = provider->taiMinusUtcSeconds(utcEpoch);
        if (!options.allowDegradedLeapSecondFallback) {
            result.status = TimeScaleConversionStatus::Failed;
            result.addWarning(TimeScaleConversionWarningCode::EpochOutsideLeapSecondTable);
            result.diagnosticText = "UTC epoch is outside the leap-second table validity range.";
            return result;
        }

        result.offsetSeconds = tableOffset.value_or(options.fallbackTaiMinusUtcSeconds);
        result.status = TimeScaleConversionStatus::Degraded;
        result.addWarning(TimeScaleConversionWarningCode::EpochOutsideLeapSecondTable);
        result.addWarning(TimeScaleConversionWarningCode::LeapSecondFallbackApplied);
        result.diagnosticText = "UTC epoch used a degraded leap-second range fallback.";
        return result;
    }

    result.offsetSeconds = provider->taiMinusUtcSeconds(utcEpoch);
    if (!result.offsetSeconds.has_value()) {
        return fallbackOffset(
            options,
            TimeScaleConversionWarningCode::EpochOutsideLeapSecondTable,
            "UTC epoch is before the first leap-second table entry."
        );
    }

    return result;
}

[[nodiscard]] OffsetLookupResult lookupTaiOffset(
    const std::shared_ptr<const ILeapSecondProvider>& provider,
    const TimeScaleServiceOptions& options,
    const AstronomicalEpoch& taiEpoch
)
{
    if (provider == nullptr || !provider->tableInfo().isUsable()) {
        return fallbackOffset(
            options,
            TimeScaleConversionWarningCode::LeapSecondTableMissing,
            "Leap-second table is unavailable for TAI conversion."
        );
    }

    OffsetLookupResult result;
    const LeapSecondTableInfo& tableInfo = provider->tableInfo();
    if (tableInfo.status == LeapSecondTableStatus::Stale) {
        result.status = TimeScaleConversionStatus::Degraded;
        result.addWarning(TimeScaleConversionWarningCode::LeapSecondTableStale);
        result.diagnosticText = "Leap-second table is stale for TAI conversion.";
    }

    const double requestedKey = taiEpoch.sortKey();
    std::optional<int> offset;
    for (const LeapSecondTableEntry& entry : provider->entries()) {
        const AstronomicalEpoch entryTaiEpoch =
            addSeconds(entry.effectiveUtcEpoch, static_cast<double>(entry.taiMinusUtcSeconds), TimeScale::Tai);
        if (entryTaiEpoch.sortKey() > requestedKey) {
            break;
        }
        offset = entry.taiMinusUtcSeconds;
    }

    if (tableInfo.validityRange.has_value() && taiEpochOutsideUtcRange(taiEpoch, *tableInfo.validityRange, provider)) {
        if (!options.allowDegradedLeapSecondFallback) {
            result.status = TimeScaleConversionStatus::Failed;
            result.addWarning(TimeScaleConversionWarningCode::EpochOutsideLeapSecondTable);
            result.diagnosticText = "TAI epoch is outside the leap-second table validity range.";
            return result;
        }

        result.offsetSeconds = offset.value_or(options.fallbackTaiMinusUtcSeconds);
        result.status = TimeScaleConversionStatus::Degraded;
        result.addWarning(TimeScaleConversionWarningCode::EpochOutsideLeapSecondTable);
        result.addWarning(TimeScaleConversionWarningCode::LeapSecondFallbackApplied);
        result.diagnosticText = "TAI epoch used a degraded leap-second range fallback.";
        return result;
    }

    if (!offset.has_value()) {
        return fallbackOffset(
            options,
            TimeScaleConversionWarningCode::EpochOutsideLeapSecondTable,
            "TAI epoch is before the first leap-second table entry."
        );
    }

    result.offsetSeconds = *offset;
    return result;
}

void mergeOffsetWarnings(TimeScaleConversionResult& result, const OffsetLookupResult& lookup) noexcept
{
    result.warningCodeMask |= lookup.warningCodeMask;
    if (lookup.status == TimeScaleConversionStatus::Degraded && result.status == TimeScaleConversionStatus::Valid) {
        result.status = TimeScaleConversionStatus::Degraded;
    }
}

void mergeUt1OffsetWarnings(TimeScaleConversionResult& result, const Ut1OffsetLookupResult& lookup) noexcept
{
    result.warningCodeMask |= lookup.warningCodeMask;
    if (lookup.status == TimeScaleConversionStatus::Degraded && result.status == TimeScaleConversionStatus::Valid) {
        result.status = TimeScaleConversionStatus::Degraded;
    }
}

void mergeConversionWarnings(TimeScaleConversionResult& result, const TimeScaleConversionResult& source) noexcept
{
    result.warningCodeMask |= source.warningCodeMask;
    if (source.status == TimeScaleConversionStatus::Degraded && result.status == TimeScaleConversionStatus::Valid) {
        result.status = TimeScaleConversionStatus::Degraded;
    }
}

void addTdbApproximationWarning(TimeScaleConversionResult& result) noexcept
{
    result.status = TimeScaleConversionStatus::Degraded;
    result.addWarning(TimeScaleConversionWarningCode::TdbApproximationApplied);
}

[[nodiscard]] TimeScaleConversionWarningCode
timeScaleWarningFromEarthOrientationWarning(const EarthOrientationSampleWarningCode code) noexcept
{
    switch (code) {
    case EarthOrientationSampleWarningCode::StaleData:
        return TimeScaleConversionWarningCode::EarthOrientationDataStale;
    case EarthOrientationSampleWarningCode::PredictedData:
        return TimeScaleConversionWarningCode::EarthOrientationDataPredicted;
    case EarthOrientationSampleWarningCode::MissingData:
        return TimeScaleConversionWarningCode::EarthOrientationDataMissing;
    case EarthOrientationSampleWarningCode::EpochOutsideRange:
        return TimeScaleConversionWarningCode::EpochOutsideEarthOrientationData;
    case EarthOrientationSampleWarningCode::InvalidInput:
        return TimeScaleConversionWarningCode::InvalidInput;
    case EarthOrientationSampleWarningCode::EstimatedData:
        return TimeScaleConversionWarningCode::EarthOrientationDataEstimated;
    }

    return TimeScaleConversionWarningCode::EarthOrientationDataMissing;
}

void mergeEarthOrientationSampleWarnings(Ut1OffsetLookupResult& result, const EarthOrientationSample& sample) noexcept
{
    constexpr std::array kSampleWarningCodes{
        EarthOrientationSampleWarningCode::StaleData,
        EarthOrientationSampleWarningCode::PredictedData,
        EarthOrientationSampleWarningCode::MissingData,
        EarthOrientationSampleWarningCode::EpochOutsideRange,
        EarthOrientationSampleWarningCode::InvalidInput,
        EarthOrientationSampleWarningCode::EstimatedData,
    };

    for (const EarthOrientationSampleWarningCode sampleCode : kSampleWarningCodes) {
        if (sample.hasWarning(sampleCode)) {
            result.addWarning(timeScaleWarningFromEarthOrientationWarning(sampleCode));
        }
    }
}

[[nodiscard]] Ut1OffsetLookupResult eopUt1Offset(
    const std::shared_ptr<const IEarthOrientationProvider>& earthOrientationProvider,
    const TimeScaleServiceOptions& options,
    const AstronomicalEpoch& utcEpoch
)
{
    const EarthOrientationSample sample =
        sampleEarthOrientation(earthOrientationProvider, utcEpoch, options.earthOrientationSampleOptions);

    Ut1OffsetLookupResult result;
    result.diagnosticText = sample.diagnosticText;
    mergeEarthOrientationSampleWarnings(result, sample);
    if (!sample.isSuccess()) {
        result.status = TimeScaleConversionStatus::Failed;
        return result;
    }

    result.ut1MinusUtcSeconds = sample.ut1MinusUtcSeconds;
    result.status = sample.status == EarthOrientationSampleStatus::Degraded ? TimeScaleConversionStatus::Degraded
                                                                            : TimeScaleConversionStatus::Valid;
    return result;
}

[[nodiscard]] Ut1OffsetLookupResult deltaTUt1Offset(
    const std::shared_ptr<const ILeapSecondProvider>& leapSecondProvider,
    const std::shared_ptr<const IDeltaTProvider>& deltaTProvider,
    const TimeScaleServiceOptions& options,
    const AstronomicalEpoch& utcEpoch,
    Ut1OffsetLookupResult sourceFailure = {}
)
{
    Ut1OffsetLookupResult result = std::move(sourceFailure);
    if (!options.allowUt1DeltaTFallback) {
        if (result.diagnosticText.empty()) {
            result.diagnosticText = "UT1 conversion requires usable Earth-orientation data.";
        }
        result.status = TimeScaleConversionStatus::Failed;
        return result;
    }

    if (deltaTProvider == nullptr) {
        result.status = TimeScaleConversionStatus::Failed;
        result.addWarning(TimeScaleConversionWarningCode::DeltaTUnavailable);
        result.diagnosticText = "UT1 conversion could not use Delta T fallback because data is unavailable.";
        return result;
    }

    const DeltaTEstimate estimate = deltaTProvider->deltaTSeconds(utcEpoch);
    if (!estimate.isUsable() || !estimate.deltaTSeconds.has_value()) {
        result.status = TimeScaleConversionStatus::Failed;
        result.addWarning(TimeScaleConversionWarningCode::DeltaTUnavailable);
        result.diagnosticText = estimate.diagnosticText.empty()
                                    ? "UT1 conversion could not use Delta T fallback for the requested epoch."
                                    : estimate.diagnosticText;
        return result;
    }

    const OffsetLookupResult utcOffset = lookupUtcOffset(leapSecondProvider, options, utcEpoch);
    if (!utcOffset.offsetSeconds.has_value()) {
        result.status = TimeScaleConversionStatus::Failed;
        result.warningCodeMask |= utcOffset.warningCodeMask;
        result.diagnosticText = utcOffset.diagnosticText;
        return result;
    }

    result.ut1MinusUtcSeconds =
        static_cast<double>(*utcOffset.offsetSeconds) + TimeConstants::kTtMinusTaiSeconds - *estimate.deltaTSeconds;
    result.status = TimeScaleConversionStatus::Degraded;
    result.warningCodeMask |= utcOffset.warningCodeMask;
    result.addWarning(TimeScaleConversionWarningCode::DeltaTFallbackApplied);
    if (estimate.status == DeltaTEstimateStatus::Degraded) {
        result.addWarning(TimeScaleConversionWarningCode::DeltaTFallbackApplied);
    }
    result.diagnosticText =
        estimate.diagnosticText.empty() ? "UT1 conversion used Delta T fallback metadata." : estimate.diagnosticText;
    return result;
}

[[nodiscard]] Ut1OffsetLookupResult lookupUtcToUt1Offset(
    const std::shared_ptr<const ILeapSecondProvider>& leapSecondProvider,
    const std::shared_ptr<const IEarthOrientationProvider>& earthOrientationProvider,
    const std::shared_ptr<const IDeltaTProvider>& deltaTProvider,
    const TimeScaleServiceOptions& options,
    const AstronomicalEpoch& utcEpoch
)
{
    Ut1OffsetLookupResult result = eopUt1Offset(earthOrientationProvider, options, utcEpoch);
    if (result.isSuccess()) {
        return result;
    }

    return deltaTUt1Offset(leapSecondProvider, deltaTProvider, options, utcEpoch, std::move(result));
}

[[nodiscard]] TimeScaleConversionResult utcFromUt1WithDeltaT(
    const std::shared_ptr<const ILeapSecondProvider>& leapSecondProvider,
    const std::shared_ptr<const IDeltaTProvider>& deltaTProvider,
    const TimeScaleServiceOptions& options,
    const AstronomicalEpoch& ut1Epoch,
    Ut1OffsetLookupResult sourceFailure
)
{
    if (!options.allowUt1DeltaTFallback || deltaTProvider == nullptr) {
        TimeScaleConversionResult result =
            failureResult(ut1Epoch, TimeScale::Utc, std::move(sourceFailure.diagnosticText));
        result.warningCodeMask = sourceFailure.warningCodeMask;
        if (deltaTProvider == nullptr) {
            result.addWarning(TimeScaleConversionWarningCode::DeltaTUnavailable);
        }
        return result;
    }

    AstronomicalEpoch estimateEpoch = ut1Epoch;
    estimateEpoch.timeScale = TimeScale::Utc;
    const DeltaTEstimate estimate = deltaTProvider->deltaTSeconds(estimateEpoch);
    if (!estimate.isUsable() || !estimate.deltaTSeconds.has_value()) {
        TimeScaleConversionResult result = failureResult(
            ut1Epoch,
            TimeScale::Utc,
            estimate.diagnosticText.empty() ? "UT1 conversion could not use Delta T fallback for the requested epoch."
                                            : estimate.diagnosticText
        );
        result.warningCodeMask = sourceFailure.warningCodeMask;
        result.addWarning(TimeScaleConversionWarningCode::DeltaTUnavailable);
        return result;
    }

    const AstronomicalEpoch ttEpoch = addSeconds(ut1Epoch, *estimate.deltaTSeconds, TimeScale::Tt);
    const AstronomicalEpoch taiEpoch = addSeconds(ttEpoch, -TimeConstants::kTtMinusTaiSeconds, TimeScale::Tai);
    const OffsetLookupResult taiOffset = lookupTaiOffset(leapSecondProvider, options, taiEpoch);
    if (!taiOffset.offsetSeconds.has_value()) {
        TimeScaleConversionResult result = failureResult(taiEpoch, TimeScale::Utc, taiOffset.diagnosticText);
        result.warningCodeMask = sourceFailure.warningCodeMask | taiOffset.warningCodeMask;
        return result;
    }

    TimeScaleConversionResult result = successResult(
        addSeconds(taiEpoch, -static_cast<double>(*taiOffset.offsetSeconds), TimeScale::Utc),
        TimeScaleConversionStatus::Degraded,
        sourceFailure.warningCodeMask | taiOffset.warningCodeMask,
        estimate.diagnosticText.empty() ? "UT1 conversion used Delta T fallback metadata." : estimate.diagnosticText
    );
    result.addWarning(TimeScaleConversionWarningCode::DeltaTFallbackApplied);
    return result;
}

[[nodiscard]] TimeScaleConversionResult lookupUtcFromUt1(
    const std::shared_ptr<const ILeapSecondProvider>& leapSecondProvider,
    const std::shared_ptr<const IEarthOrientationProvider>& earthOrientationProvider,
    const std::shared_ptr<const IDeltaTProvider>& deltaTProvider,
    const TimeScaleServiceOptions& options,
    const AstronomicalEpoch& ut1Epoch
)
{
    AstronomicalEpoch utcEpoch = ut1Epoch;
    utcEpoch.timeScale = TimeScale::Utc;
    Ut1OffsetLookupResult lookup;
    for (int iteration = 0; iteration < 4; ++iteration) {
        lookup = eopUt1Offset(earthOrientationProvider, options, utcEpoch);
        if (!lookup.ut1MinusUtcSeconds.has_value()) {
            return utcFromUt1WithDeltaT(leapSecondProvider, deltaTProvider, options, ut1Epoch, std::move(lookup));
        }
        utcEpoch = addSeconds(ut1Epoch, -*lookup.ut1MinusUtcSeconds, TimeScale::Utc);
    }

    TimeScaleConversionResult result =
        successResult(utcEpoch, lookup.status, lookup.warningCodeMask, lookup.diagnosticText);
    mergeUt1OffsetWarnings(result, lookup);
    return result;
}

}  // namespace

LeapSecondTimeScaleService::LeapSecondTimeScaleService(
    std::shared_ptr<const ILeapSecondProvider> leapSecondProvider,
    TimeScaleServiceOptions options,
    std::shared_ptr<const IEarthOrientationProvider> earthOrientationProvider,
    std::shared_ptr<const IDeltaTProvider> deltaTProvider
)
    : m_leapSecondProvider(std::move(leapSecondProvider)),
      m_earthOrientationProvider(std::move(earthOrientationProvider)), m_deltaTProvider(std::move(deltaTProvider)),
      m_options(options)
{
}

TimeScaleConversionResult
LeapSecondTimeScaleService::convert(const AstronomicalEpoch& epoch, const TimeScale targetScale) const
{
    if (!epoch.isFinite()) {
        TimeScaleConversionResult result = failureResult(epoch, targetScale, "Time-scale conversion input is invalid.");
        result.addWarning(TimeScaleConversionWarningCode::InvalidInput);
        return result;
    }

    if (epoch.timeScale == targetScale) {
        return successResult(epoch);
    }

    switch (epoch.timeScale) {
    case TimeScale::Utc: {
        if (targetScale == TimeScale::Ut1) {
            const Ut1OffsetLookupResult lookup = lookupUtcToUt1Offset(
                m_leapSecondProvider, m_earthOrientationProvider, m_deltaTProvider, m_options, epoch
            );
            if (!lookup.ut1MinusUtcSeconds.has_value()) {
                TimeScaleConversionResult result = failureResult(epoch, targetScale, lookup.diagnosticText);
                result.warningCodeMask = lookup.warningCodeMask;
                return result;
            }

            TimeScaleConversionResult result =
                successResult(addSeconds(epoch, *lookup.ut1MinusUtcSeconds, TimeScale::Ut1));
            mergeUt1OffsetWarnings(result, lookup);
            if (!lookup.diagnosticText.empty()) {
                result.diagnosticText = lookup.diagnosticText;
            }
            return result;
        }

        if (targetScale == TimeScale::Tdb) {
            const TimeScaleConversionResult tt = convert(epoch, TimeScale::Tt);
            if (!tt.isSuccess()) {
                TimeScaleConversionResult result = tt;
                result.epoch.timeScale = targetScale;
                return result;
            }

            TimeScaleConversionResult tdb = convert(tt.epoch, TimeScale::Tdb);
            mergeConversionWarnings(tdb, tt);
            return tdb;
        }

        if (targetScale != TimeScale::Tai && targetScale != TimeScale::Tt) {
            TimeScaleConversionResult result =
                failureResult(epoch, targetScale, "Only UTC to TAI, TT, or TDB conversion is currently supported.");
            result.addWarning(TimeScaleConversionWarningCode::UnsupportedConversion);
            return result;
        }

        const OffsetLookupResult lookup = lookupUtcOffset(m_leapSecondProvider, m_options, epoch);
        if (!lookup.offsetSeconds.has_value()) {
            TimeScaleConversionResult result = failureResult(epoch, targetScale, lookup.diagnosticText);
            result.warningCodeMask = lookup.warningCodeMask;
            return result;
        }

        const double taiOffset = static_cast<double>(*lookup.offsetSeconds);
        const double targetOffset =
            targetScale == TimeScale::Tt ? taiOffset + TimeConstants::kTtMinusTaiSeconds : taiOffset;
        TimeScaleConversionResult result =
            successResult(addSeconds(epoch, targetOffset, targetScale), lookup.status, lookup.warningCodeMask);
        if (!lookup.diagnosticText.empty()) {
            result.diagnosticText = lookup.diagnosticText;
        }
        return result;
    }
    case TimeScale::Tai:
        if (targetScale == TimeScale::Tdb) {
            TimeScaleConversionResult tt = convert(epoch, TimeScale::Tt);
            if (!tt.isSuccess()) {
                TimeScaleConversionResult result = tt;
                result.epoch.timeScale = targetScale;
                return result;
            }

            TimeScaleConversionResult tdb = convert(tt.epoch, TimeScale::Tdb);
            mergeConversionWarnings(tdb, tt);
            return tdb;
        }
        if (targetScale == TimeScale::Tt) {
            return successResult(addSeconds(epoch, TimeConstants::kTtMinusTaiSeconds, TimeScale::Tt));
        }
        if (targetScale == TimeScale::Utc) {
            const OffsetLookupResult lookup = lookupTaiOffset(m_leapSecondProvider, m_options, epoch);
            if (!lookup.offsetSeconds.has_value()) {
                TimeScaleConversionResult result = failureResult(epoch, targetScale, lookup.diagnosticText);
                result.warningCodeMask = lookup.warningCodeMask;
                return result;
            }

            TimeScaleConversionResult result =
                successResult(addSeconds(epoch, -static_cast<double>(*lookup.offsetSeconds), TimeScale::Utc));
            mergeOffsetWarnings(result, lookup);
            if (!lookup.diagnosticText.empty()) {
                result.diagnosticText = lookup.diagnosticText;
            }
            return result;
        }
        if (targetScale == TimeScale::Ut1) {
            TimeScaleConversionResult utc = convert(epoch, TimeScale::Utc);
            if (!utc.isSuccess()) {
                utc.epoch.timeScale = targetScale;
                return utc;
            }

            TimeScaleConversionResult ut1 = convert(utc.epoch, TimeScale::Ut1);
            mergeConversionWarnings(ut1, utc);
            return ut1;
        }
        break;
    case TimeScale::Tt:
        if (targetScale == TimeScale::Ut1) {
            TimeScaleConversionResult utc = convert(epoch, TimeScale::Utc);
            if (!utc.isSuccess()) {
                utc.epoch.timeScale = targetScale;
                return utc;
            }

            TimeScaleConversionResult ut1 = convert(utc.epoch, TimeScale::Ut1);
            mergeConversionWarnings(ut1, utc);
            return ut1;
        }
        if (targetScale == TimeScale::Tdb) {
            const std::optional<double> tdbMinusTt = tdbMinusTtSeconds(epoch);
            if (!tdbMinusTt.has_value()) {
                TimeScaleConversionResult result =
                    failureResult(epoch, targetScale, "TT to TDB conversion input is invalid.");
                result.addWarning(TimeScaleConversionWarningCode::InvalidInput);
                return result;
            }

            TimeScaleConversionResult result = successResult(
                addSeconds(epoch, *tdbMinusTt, TimeScale::Tdb),
                TimeScaleConversionStatus::Degraded,
                timeScaleConversionWarningMask(TimeScaleConversionWarningCode::TdbApproximationApplied),
                "TT to TDB conversion used the configured high-precision approximation."
            );
            addTdbApproximationWarning(result);
            return result;
        }
        if (targetScale == TimeScale::Tai) {
            return successResult(addSeconds(epoch, -TimeConstants::kTtMinusTaiSeconds, TimeScale::Tai));
        }
        if (targetScale == TimeScale::Utc) {
            return convert(addSeconds(epoch, -TimeConstants::kTtMinusTaiSeconds, TimeScale::Tai), TimeScale::Utc);
        }
        break;
    case TimeScale::Tdb: {
        AstronomicalEpoch tt = epoch;
        tt.timeScale = TimeScale::Tt;
        for (int iteration = 0; iteration < 3; ++iteration) {
            const std::optional<double> tdbMinusTt = tdbMinusTtSeconds(tt);
            if (!tdbMinusTt.has_value()) {
                TimeScaleConversionResult result =
                    failureResult(epoch, targetScale, "TDB to TT conversion input is invalid.");
                result.addWarning(TimeScaleConversionWarningCode::InvalidInput);
                return result;
            }
            tt = addSeconds(epoch, -*tdbMinusTt, TimeScale::Tt);
        }

        TimeScaleConversionResult result = successResult(
            tt,
            TimeScaleConversionStatus::Degraded,
            timeScaleConversionWarningMask(TimeScaleConversionWarningCode::TdbApproximationApplied),
            "TDB to TT conversion used the configured high-precision approximation."
        );
        addTdbApproximationWarning(result);
        if (targetScale == TimeScale::Tt) {
            return result;
        }

        TimeScaleConversionResult converted = convert(result.epoch, targetScale);
        mergeConversionWarnings(converted, result);
        return converted;
    }
    case TimeScale::Ut1: {
        TimeScaleConversionResult utc =
            lookupUtcFromUt1(m_leapSecondProvider, m_earthOrientationProvider, m_deltaTProvider, m_options, epoch);
        if (targetScale == TimeScale::Utc) {
            return utc;
        }
        if (!utc.isSuccess()) {
            utc.epoch.timeScale = targetScale;
            return utc;
        }

        TimeScaleConversionResult converted = convert(utc.epoch, targetScale);
        mergeConversionWarnings(converted, utc);
        return converted;
    }
    }

    TimeScaleConversionResult result =
        failureResult(epoch, targetScale, "The requested time-scale conversion is not supported.");
    result.addWarning(TimeScaleConversionWarningCode::UnsupportedConversion);
    return result;
}

TimeScaleConversionResult
LeapSecondTimeScaleService::convertCivilDateTime(const CivilDateTime& dateTime, const TimeScale targetScale) const
{
    if (!CalendarTime::isValidCivilDateTime(dateTime)) {
        AstronomicalEpoch epoch;
        epoch.timeScale = targetScale;
        TimeScaleConversionResult result = failureResult(epoch, targetScale, "Civil date/time input is invalid.");
        result.addWarning(TimeScaleConversionWarningCode::InvalidInput);
        return result;
    }

    std::optional<AstronomicalEpoch> epoch;
    if (!isUtcLeapSecondLabel(dateTime)) {
        epoch = CalendarTime::astronomicalEpochFromCivilDateTime(dateTime);
    } else {
        CivilDateTime precedingSecond = dateTime;
        precedingSecond.second = 59;
        const std::optional<AstronomicalEpoch> precedingEpoch =
            CalendarTime::astronomicalEpochFromCivilDateTime(precedingSecond);
        if (precedingEpoch.has_value()) {
            epoch = addSeconds(*precedingEpoch, 1.0, TimeScale::Utc);
        }
    }

    if (!epoch.has_value()) {
        AstronomicalEpoch failedEpoch;
        failedEpoch.timeScale = targetScale;
        TimeScaleConversionResult result =
            failureResult(failedEpoch, targetScale, "Civil date/time input could not be converted to an epoch.");
        result.addWarning(TimeScaleConversionWarningCode::InvalidInput);
        return result;
    }

    if (!isUtcLeapSecondLabel(dateTime)) {
        return convert(*epoch, targetScale);
    }

    CivilDateTime precedingSecond = dateTime;
    precedingSecond.second = 59;
    const std::optional<AstronomicalEpoch> precedingEpoch =
        CalendarTime::astronomicalEpochFromCivilDateTime(precedingSecond);
    if (!precedingEpoch.has_value()) {
        TimeScaleConversionResult result =
            failureResult(*epoch, targetScale, "Leap-second civil date/time input is invalid.");
        result.addWarning(TimeScaleConversionWarningCode::InvalidInput);
        return result;
    }

    const OffsetLookupResult precedingLookup = lookupUtcOffset(m_leapSecondProvider, m_options, *precedingEpoch);
    const OffsetLookupResult nextLookup = lookupUtcOffset(m_leapSecondProvider, m_options, *epoch);
    if (!precedingLookup.offsetSeconds.has_value() || !nextLookup.offsetSeconds.has_value()
        || *nextLookup.offsetSeconds != *precedingLookup.offsetSeconds + 1) {
        TimeScaleConversionResult result =
            failureResult(*epoch, targetScale, "Civil date/time does not identify a configured positive leap second.");
        result.warningCodeMask = precedingLookup.warningCodeMask | nextLookup.warningCodeMask;
        result.addWarning(TimeScaleConversionWarningCode::InvalidInput);
        return result;
    }

    if (targetScale == TimeScale::Utc) {
        return successResult(*epoch);
    }
    if (targetScale != TimeScale::Tai && targetScale != TimeScale::Tt && targetScale != TimeScale::Tdb) {
        TimeScaleConversionResult result = failureResult(
            *epoch, targetScale, "Only UTC leap-second conversion to TAI, TT, or TDB is currently supported."
        );
        result.addWarning(TimeScaleConversionWarningCode::UnsupportedConversion);
        return result;
    }

    const double taiOffset = static_cast<double>(*precedingLookup.offsetSeconds);
    const double targetOffset =
        targetScale == TimeScale::Tai ? taiOffset : taiOffset + TimeConstants::kTtMinusTaiSeconds;
    TimeScaleConversionResult result =
        successResult(addSeconds(*epoch, targetOffset, targetScale == TimeScale::Tdb ? TimeScale::Tt : targetScale));
    mergeOffsetWarnings(result, precedingLookup);
    mergeOffsetWarnings(result, nextLookup);
    if (targetScale == TimeScale::Tdb) {
        TimeScaleConversionResult tdb = convert(result.epoch, TimeScale::Tdb);
        mergeConversionWarnings(tdb, result);
        return tdb;
    }
    return result;
}

}  // namespace skygate::ephemeris
