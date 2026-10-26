#include "LeapSecondOffsetResolver.hpp"
#include "LeapSecondTableScan.hpp"
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

/// Failure result for a provider that carries no usable table. The lookup of
/// either epoch domain reports the same missing-table fallback.
[[nodiscard]] std::optional<LeapSecondOffsetResolver::Result> unusableTableOffset(
    const std::shared_ptr<const ILeapSecondProvider>& provider,
    const TimeScaleServiceOptions& options,
    std::string diagnosticText
)
{
    if (provider != nullptr && provider->tableInfo().isUsable()) {
        return std::nullopt;
    }

    return fallbackOffset(
        options, TimeScaleConversionWarningCode::Type::LeapSecondTableMissing, std::move(diagnosticText)
    );
}

/// Marks a result degraded when the table is stale and describes the lookup.
void applyStaleTableWarning(
    LeapSecondOffsetResolver::Result& result,
    const ILeapSecondProvider::TableInfo& tableInfo,
    std::string diagnosticText
)
{
    if (tableInfo.status != ILeapSecondProvider::TableStatus::Stale) {
        return;
    }

    result.status = TimeScaleConversionStatus::Type::Degraded;
    result.addWarning(TimeScaleConversionWarningCode::Type::LeapSecondTableStale);
    result.diagnosticText = std::move(diagnosticText);
}

/// Result of a lookup whose epoch falls outside the table validity range: a
/// failure, or the degraded range fallback of the options. The result carries
/// the table-state warnings the lookup collected before the range check.
[[nodiscard]] LeapSecondOffsetResolver::Result outOfRangeOffset(
    LeapSecondOffsetResolver::Result result,
    const TimeScaleServiceOptions& options,
    const std::optional<double> tableOffsetSeconds,
    std::string failDiagnosticText,
    std::string degradedDiagnosticText
)
{
    result.addWarning(TimeScaleConversionWarningCode::Type::EpochOutsideLeapSecondTable);
    if (!options.allowDegradedLeapSecondFallback) {
        result.status = TimeScaleConversionStatus::Type::Failed;
        result.diagnosticText = std::move(failDiagnosticText);
        return result;
    }

    result.offsetSeconds = tableOffsetSeconds.value_or(static_cast<double>(options.fallbackTaiMinusUtcSeconds));
    result.status = TimeScaleConversionStatus::Type::Degraded;
    result.addWarning(TimeScaleConversionWarningCode::Type::LeapSecondFallbackApplied);
    result.diagnosticText = std::move(degradedDiagnosticText);
    return result;
}

[[nodiscard]] std::optional<double> seconds(const std::optional<int> offsetSeconds) noexcept
{
    return offsetSeconds.has_value() ? std::optional<double>{static_cast<double>(*offsetSeconds)} : std::nullopt;
}

[[nodiscard]] LeapSecondOffsetResolver::Result lookupUtcOffset(
    const std::shared_ptr<const ILeapSecondProvider>& provider,
    const TimeScaleServiceOptions& options,
    const skygate::core::AstronomicalEpoch& utcEpoch
)
{
    if (std::optional<LeapSecondOffsetResolver::Result> unusable =
            unusableTableOffset(provider, options, "Leap-second table is unavailable for UTC conversion.");
        unusable.has_value()) {
        return std::move(*unusable);
    }

    LeapSecondOffsetResolver::Result result;
    const ILeapSecondProvider::TableInfo& tableInfo = provider->tableInfo();
    applyStaleTableWarning(result, tableInfo, "Leap-second table is stale for UTC conversion.");

    if (tableInfo.validityRange.has_value() && !tableInfo.validityRange->contains(utcEpoch)) {
        return outOfRangeOffset(
            std::move(result),
            options,
            seconds(provider->taiMinusUtcSeconds(utcEpoch)),
            "UTC epoch is outside the leap-second table validity range.",
            "UTC epoch used a degraded leap-second range fallback."
        );
    }

    result.offsetSeconds = seconds(provider->taiMinusUtcSeconds(utcEpoch));
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
    if (std::optional<LeapSecondOffsetResolver::Result> unusable =
            unusableTableOffset(provider, options, "Leap-second table is unavailable for TAI conversion.");
        unusable.has_value()) {
        return std::move(*unusable);
    }

    LeapSecondOffsetResolver::Result result;
    const ILeapSecondProvider::TableInfo& tableInfo = provider->tableInfo();
    applyStaleTableWarning(result, tableInfo, "Leap-second table is stale for TAI conversion.");

    const std::optional<int> offset = LeapSecondTableScan::offsetAtTaiKey(provider->entries(), taiEpoch.sortKey());
    if (tableInfo.validityRange.has_value() && taiEpochOutsideUtcRange(taiEpoch, *tableInfo.validityRange, provider)) {
        return outOfRangeOffset(
            std::move(result),
            options,
            seconds(offset),
            "TAI epoch is outside the leap-second table validity range.",
            "TAI epoch used a degraded leap-second range fallback."
        );
    }

    if (!offset.has_value()) {
        return fallbackOffset(
            options,
            TimeScaleConversionWarningCode::Type::EpochOutsideLeapSecondTable,
            "TAI epoch is before the first leap-second table entry."
        );
    }

    result.offsetSeconds = seconds(offset);
    return result;
}

}  // namespace

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

}  // namespace skygate::ephemeris
