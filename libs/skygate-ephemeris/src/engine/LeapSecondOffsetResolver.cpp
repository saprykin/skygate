#include "LeapSecondOffsetResolver.hpp"
#include "BitFlagSetOperations.hpp"
#include "TimeScaleConversionWarningCode.hpp"

#include <optional>
#include <string>
#include <utility>

namespace skygate::ephemeris {

namespace {

[[nodiscard]] bool taiEpochOutsideUtcRange(
    const skygate::core::AstronomicalEpoch& taiEpoch,
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
    const skygate::core::AstronomicalEpoch startTaiEpoch =
        utcRange.start.addSeconds(static_cast<double>(*startOffsetSeconds), skygate::core::TimeScale::Tai);
    const skygate::core::AstronomicalEpoch endTaiEpoch =
        utcRange.end.addSeconds(static_cast<double>(*endOffsetSeconds), skygate::core::TimeScale::Tai);
    return key < startTaiEpoch.sortKey() || key > endTaiEpoch.sortKey();
}

[[nodiscard]] LeapSecondOffsetResolver::Result fallbackOffset(
    const TimeScaleServiceOptions& options, TimeScaleConversionWarningCode::Type warningCode, std::string diagnosticText
)
{
    LeapSecondOffsetResolver::Result result;
    if (!options.allowDegradedLeapSecondFallback) {
        result.status = TimeScaleConversionStatus::Type::Failed;
        result.diagnosticText = std::move(diagnosticText);
        result.addWarning(warningCode);
        return result;
    }

    result.offsetSeconds = options.fallbackTaiMinusUtcSeconds;
    result.status = TimeScaleConversionStatus::Type::Degraded;
    result.addWarning(warningCode);
    result.addWarning(TimeScaleConversionWarningCode::Type::LeapSecondFallbackApplied);
    result.diagnosticText = "Time-scale conversion used a degraded leap-second fallback.";
    return result;
}

[[nodiscard]] LeapSecondOffsetResolver::Result lookupUtcOffset(
    const std::shared_ptr<const ILeapSecondProvider>& provider,
    const TimeScaleServiceOptions& options,
    const skygate::core::AstronomicalEpoch& utcEpoch
)
{
    if (provider == nullptr || !provider->tableInfo().isUsable()) {
        return fallbackOffset(
            options,
            TimeScaleConversionWarningCode::Type::LeapSecondTableMissing,
            "Leap-second table is unavailable for UTC conversion."
        );
    }

    LeapSecondOffsetResolver::Result result;
    const ILeapSecondProvider::TableInfo& tableInfo = provider->tableInfo();
    if (tableInfo.status == ILeapSecondProvider::TableStatus::Stale) {
        result.status = TimeScaleConversionStatus::Type::Degraded;
        result.addWarning(TimeScaleConversionWarningCode::Type::LeapSecondTableStale);
        result.diagnosticText = "Leap-second table is stale for UTC conversion.";
    }

    if (tableInfo.validityRange.has_value() && !tableInfo.validityRange->contains(utcEpoch)) {
        const std::optional<int> tableOffset = provider->taiMinusUtcSeconds(utcEpoch);
        if (!options.allowDegradedLeapSecondFallback) {
            result.status = TimeScaleConversionStatus::Type::Failed;
            result.addWarning(TimeScaleConversionWarningCode::Type::EpochOutsideLeapSecondTable);
            result.diagnosticText = "UTC epoch is outside the leap-second table validity range.";
            return result;
        }

        result.offsetSeconds = tableOffset.value_or(options.fallbackTaiMinusUtcSeconds);
        result.status = TimeScaleConversionStatus::Type::Degraded;
        result.addWarning(TimeScaleConversionWarningCode::Type::EpochOutsideLeapSecondTable);
        result.addWarning(TimeScaleConversionWarningCode::Type::LeapSecondFallbackApplied);
        result.diagnosticText = "UTC epoch used a degraded leap-second range fallback.";
        return result;
    }

    result.offsetSeconds = provider->taiMinusUtcSeconds(utcEpoch);
    if (!result.offsetSeconds.has_value()) {
        return fallbackOffset(
            options,
            TimeScaleConversionWarningCode::Type::EpochOutsideLeapSecondTable,
            "UTC epoch is before the first leap-second table entry."
        );
    }

    return result;
}

[[nodiscard]] LeapSecondOffsetResolver::Result lookupTaiOffset(
    const std::shared_ptr<const ILeapSecondProvider>& provider,
    const TimeScaleServiceOptions& options,
    const skygate::core::AstronomicalEpoch& taiEpoch
)
{
    if (provider == nullptr || !provider->tableInfo().isUsable()) {
        return fallbackOffset(
            options,
            TimeScaleConversionWarningCode::Type::LeapSecondTableMissing,
            "Leap-second table is unavailable for TAI conversion."
        );
    }

    LeapSecondOffsetResolver::Result result;
    const ILeapSecondProvider::TableInfo& tableInfo = provider->tableInfo();
    if (tableInfo.status == ILeapSecondProvider::TableStatus::Stale) {
        result.status = TimeScaleConversionStatus::Type::Degraded;
        result.addWarning(TimeScaleConversionWarningCode::Type::LeapSecondTableStale);
        result.diagnosticText = "Leap-second table is stale for TAI conversion.";
    }

    const double requestedKey = taiEpoch.sortKey();
    std::optional<int> offset;
    for (const ILeapSecondProvider::TableEntry& entry : provider->entries()) {
        const skygate::core::AstronomicalEpoch entryTaiEpoch = entry.effectiveUtcEpoch.addSeconds(
            static_cast<double>(entry.taiMinusUtcSeconds), skygate::core::TimeScale::Tai
        );
        if (entryTaiEpoch.sortKey() > requestedKey) {
            break;
        }
        offset = entry.taiMinusUtcSeconds;
    }

    if (tableInfo.validityRange.has_value() && taiEpochOutsideUtcRange(taiEpoch, *tableInfo.validityRange, provider)) {
        if (!options.allowDegradedLeapSecondFallback) {
            result.status = TimeScaleConversionStatus::Type::Failed;
            result.addWarning(TimeScaleConversionWarningCode::Type::EpochOutsideLeapSecondTable);
            result.diagnosticText = "TAI epoch is outside the leap-second table validity range.";
            return result;
        }

        result.offsetSeconds = offset.value_or(options.fallbackTaiMinusUtcSeconds);
        result.status = TimeScaleConversionStatus::Type::Degraded;
        result.addWarning(TimeScaleConversionWarningCode::Type::EpochOutsideLeapSecondTable);
        result.addWarning(TimeScaleConversionWarningCode::Type::LeapSecondFallbackApplied);
        result.diagnosticText = "TAI epoch used a degraded leap-second range fallback.";
        return result;
    }

    if (!offset.has_value()) {
        return fallbackOffset(
            options,
            TimeScaleConversionWarningCode::Type::EpochOutsideLeapSecondTable,
            "TAI epoch is before the first leap-second table entry."
        );
    }

    result.offsetSeconds = *offset;
    return result;
}

}  // namespace

void LeapSecondOffsetResolver::Result::addWarning(const TimeScaleConversionWarningCode::Type code) noexcept
{
    warningCodeMask |= BitFlagSetOperations::bit(code);
}

LeapSecondOffsetResolver::LeapSecondOffsetResolver(
    std::shared_ptr<const ILeapSecondProvider> leapSecondProvider, TimeScaleServiceOptions options
)
    : m_leapSecondProvider(std::move(leapSecondProvider)), m_options(options)
{
}

LeapSecondOffsetResolver::Result
LeapSecondOffsetResolver::lookupUtcOffset(const skygate::core::AstronomicalEpoch& utcEpoch) const
{
    return skygate::ephemeris::lookupUtcOffset(m_leapSecondProvider, m_options, utcEpoch);
}

LeapSecondOffsetResolver::Result
LeapSecondOffsetResolver::lookupTaiOffset(const skygate::core::AstronomicalEpoch& taiEpoch) const
{
    return skygate::ephemeris::lookupTaiOffset(m_leapSecondProvider, m_options, taiEpoch);
}

void LeapSecondOffsetResolver::mergeWarnings(TimeScaleConversionResult& result, const Result& lookup) noexcept
{
    TimeScaleConversionResult::mergeWarnings(result, lookup.status, lookup.warningCodeMask);
}

}  // namespace skygate::ephemeris
