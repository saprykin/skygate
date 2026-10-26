#pragma once

#include "EphemerisMetadataFailurePolicy.hpp"
#include "engine/TimeScaleConversionResult.hpp"
#include "time/AstronomicalEpoch.hpp"

#include <cstdint>
#include <memory>
#include <optional>

namespace skygate::ephemeris {
class ITimeScaleService;
struct EphemerisEngineQueryResult;
}  // namespace skygate::ephemeris

namespace skygate::ephemeris::highprecision {

/// Converts a request epoch into the scale a computation runs in and merges the
/// conversion diagnostics into the query result.
///
/// A missing time-scale service marks the query Failed or Degraded as the
/// failure policy asks and yields no epoch. A conversion that ran contributes
/// its warnings through the metadata merger, and a conversion that failed
/// yields no epoch. A converted epoch is returned exactly as the time-scale
/// service produced it; only a same-scale epoch is normalized.
class EpochScaleResolver final {
public:
    EpochScaleResolver() = delete;

    /// What an epoch that already is in the target scale does.
    enum class SameScaleConversion : std::uint8_t {
        // Returned normalized without asking the time-scale service.
        Skip,
        // Converted anyway, so the diagnostics of the conversion reach the
        // query. A caller picks this when the warnings of a same-scale
        // conversion are part of its contract.
        Convert,
    };

    /// Resolves the TDB epoch a kernel query runs at.
    ///
    /// The conversion diagnostics reach the query through the merger that keeps
    /// a TDB-approximation-only degradation silent. The returned epoch is always
    /// normalized.
    [[nodiscard]] static std::optional<skygate::core::AstronomicalEpoch> resolve(
        EphemerisEngineQueryResult& metadata,
        const skygate::core::AstronomicalEpoch& epoch,
        const std::shared_ptr<const skygate::ephemeris::ITimeScaleService>& timeScaleService,
        EphemerisMetadataFailurePolicy failurePolicy
    ) noexcept;

    /// Resolves 'epoch' into 'targetScale' and merges the conversion diagnostics.
    ///
    /// An epoch already in 'targetScale' is returned normalized without a
    /// conversion unless 'sameScaleConversion' asks for one. 'conversionCache'
    /// reuses a conversion across repeated calls; without a cache the resolved
    /// conversion still runs and is only local.
    [[nodiscard]] static std::optional<skygate::core::AstronomicalEpoch> resolveInScale(
        EphemerisEngineQueryResult& metadata,
        const skygate::core::AstronomicalEpoch& epoch,
        skygate::core::TimeScale targetScale,
        const skygate::ephemeris::ITimeScaleService* timeScaleService,
        EphemerisMetadataFailurePolicy failurePolicy,
        std::optional<TimeScaleConversionResult>* conversionCache = nullptr,
        SameScaleConversion sameScaleConversion = SameScaleConversion::Skip
    ) noexcept;

private:
    enum class ConversionMerge : std::uint8_t {
        StatusAndWarnings,
        KernelEpoch,
    };

    [[nodiscard]] static std::optional<skygate::core::AstronomicalEpoch> resolveConversion(
        EphemerisEngineQueryResult& metadata,
        const skygate::core::AstronomicalEpoch& epoch,
        skygate::core::TimeScale targetScale,
        const skygate::ephemeris::ITimeScaleService* timeScaleService,
        EphemerisMetadataFailurePolicy failurePolicy,
        ConversionMerge merge,
        std::optional<TimeScaleConversionResult>* conversionCache
    ) noexcept;
};

}  // namespace skygate::ephemeris::highprecision
