#include "skygate/ephemeris/TimeScaleService.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace skygate::ephemeris {

namespace {

constexpr double kSecondsPerDay = 86'400.0;
constexpr double kTtMinusTaiSeconds = 32.184;

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

[[nodiscard]] bool isFiniteEpoch(const AstronomicalEpoch& epoch) noexcept
{
    return std::isfinite(epoch.julianDatePart1) && std::isfinite(epoch.julianDatePart2);
}

[[nodiscard]] double epochSortKey(const AstronomicalEpoch& epoch) noexcept
{
    const AstronomicalEpoch normalized = normalizedAstronomicalEpoch(epoch);
    return normalized.julianDatePart1 + normalized.julianDatePart2;
}

[[nodiscard]] AstronomicalEpoch
addSeconds(const AstronomicalEpoch& epoch, const double seconds, const TimeScale targetScale) noexcept
{
    return normalizedAstronomicalEpoch(AstronomicalEpoch{
        .julianDatePart1 = epoch.julianDatePart1,
        .julianDatePart2 = epoch.julianDatePart2 + seconds / kSecondsPerDay,
        .timeScale = targetScale,
    });
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
        .epoch = normalizedAstronomicalEpoch(epoch),
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
    const double key = epochSortKey(epoch);
    return key < epochSortKey(range.start) || key > epochSortKey(range.end);
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

    const double key = epochSortKey(taiEpoch);
    const AstronomicalEpoch startTaiEpoch =
        addSeconds(utcRange.start, static_cast<double>(*startOffsetSeconds), TimeScale::Tai);
    const AstronomicalEpoch endTaiEpoch =
        addSeconds(utcRange.end, static_cast<double>(*endOffsetSeconds), TimeScale::Tai);
    return key < epochSortKey(startTaiEpoch) || key > epochSortKey(endTaiEpoch);
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

    const double requestedKey = epochSortKey(taiEpoch);
    std::optional<int> offset;
    for (const LeapSecondTableEntry& entry : provider->entries()) {
        const AstronomicalEpoch entryTaiEpoch =
            addSeconds(entry.effectiveUtcEpoch, static_cast<double>(entry.taiMinusUtcSeconds), TimeScale::Tai);
        if (epochSortKey(entryTaiEpoch) > requestedKey) {
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

}  // namespace

LeapSecondTimeScaleService::LeapSecondTimeScaleService(
    std::shared_ptr<const ILeapSecondProvider> leapSecondProvider, TimeScaleServiceOptions options
)
    : m_leapSecondProvider(std::move(leapSecondProvider)), m_options(options)
{
}

TimeScaleConversionResult
LeapSecondTimeScaleService::convert(const AstronomicalEpoch& epoch, const TimeScale targetScale) const
{
    if (!isFiniteEpoch(epoch)) {
        TimeScaleConversionResult result = failureResult(epoch, targetScale, "Time-scale conversion input is invalid.");
        result.addWarning(TimeScaleConversionWarningCode::InvalidInput);
        return result;
    }

    if (epoch.timeScale == targetScale) {
        return successResult(epoch);
    }

    switch (epoch.timeScale) {
    case TimeScale::Utc: {
        if (targetScale != TimeScale::Tai && targetScale != TimeScale::Tt) {
            TimeScaleConversionResult result =
                failureResult(epoch, targetScale, "Only UTC to TAI or TT conversion is currently supported.");
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
        const double targetOffset = targetScale == TimeScale::Tt ? taiOffset + kTtMinusTaiSeconds : taiOffset;
        TimeScaleConversionResult result =
            successResult(addSeconds(epoch, targetOffset, targetScale), lookup.status, lookup.warningCodeMask);
        if (!lookup.diagnosticText.empty()) {
            result.diagnosticText = lookup.diagnosticText;
        }
        return result;
    }
    case TimeScale::Tai:
        if (targetScale == TimeScale::Tt) {
            return successResult(addSeconds(epoch, kTtMinusTaiSeconds, TimeScale::Tt));
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
        break;
    case TimeScale::Tt:
        if (targetScale == TimeScale::Tai) {
            return successResult(addSeconds(epoch, -kTtMinusTaiSeconds, TimeScale::Tai));
        }
        if (targetScale == TimeScale::Utc) {
            return convert(addSeconds(epoch, -kTtMinusTaiSeconds, TimeScale::Tai), TimeScale::Utc);
        }
        break;
    case TimeScale::Tdb:
    case TimeScale::Ut1:
        break;
    }

    TimeScaleConversionResult result =
        failureResult(epoch, targetScale, "The requested time-scale conversion is not supported.");
    result.addWarning(TimeScaleConversionWarningCode::UnsupportedConversion);
    return result;
}

TimeScaleConversionResult
LeapSecondTimeScaleService::convertCivilDateTime(const CivilDateTime& dateTime, const TimeScale targetScale) const
{
    if (!isValidCivilDateTime(dateTime)) {
        AstronomicalEpoch epoch;
        epoch.timeScale = targetScale;
        TimeScaleConversionResult result = failureResult(epoch, targetScale, "Civil date/time input is invalid.");
        result.addWarning(TimeScaleConversionWarningCode::InvalidInput);
        return result;
    }

    std::optional<AstronomicalEpoch> epoch;
    if (!isUtcLeapSecondLabel(dateTime)) {
        epoch = astronomicalEpochFromCivilDateTime(dateTime);
    } else {
        CivilDateTime precedingSecond = dateTime;
        precedingSecond.second = 59;
        const std::optional<AstronomicalEpoch> precedingEpoch = astronomicalEpochFromCivilDateTime(precedingSecond);
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
    const std::optional<AstronomicalEpoch> precedingEpoch = astronomicalEpochFromCivilDateTime(precedingSecond);
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
    if (targetScale != TimeScale::Tai && targetScale != TimeScale::Tt) {
        TimeScaleConversionResult result =
            failureResult(*epoch, targetScale, "Only UTC leap-second conversion to TAI or TT is currently supported.");
        result.addWarning(TimeScaleConversionWarningCode::UnsupportedConversion);
        return result;
    }

    const double taiOffset = static_cast<double>(*precedingLookup.offsetSeconds);
    const double targetOffset = targetScale == TimeScale::Tt ? taiOffset + kTtMinusTaiSeconds : taiOffset;
    TimeScaleConversionResult result = successResult(addSeconds(*epoch, targetOffset, targetScale));
    mergeOffsetWarnings(result, precedingLookup);
    mergeOffsetWarnings(result, nextLookup);
    return result;
}

}  // namespace skygate::ephemeris
