#include "engine/highprecision/EphemerisResultBuilder.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace skygate::ephemeris::highprecision {
namespace {

constexpr std::string_view kHighPrecisionDataSourceProvenance = "High-precision ephemeris facade";

[[nodiscard]] CelestialBodyState makeEmptyState(const std::size_t bodyIndex) noexcept
{
    CelestialBodyState state;
    state.bodyIndex = static_cast<std::uint32_t>(bodyIndex);
    state.equatorial.rightAscensionHours = std::numeric_limits<double>::quiet_NaN();
    state.equatorial.declinationDeg = std::numeric_limits<double>::quiet_NaN();
    state.horizontal.altitudeDeg = std::numeric_limits<double>::quiet_NaN();
    state.horizontal.azimuthDeg = std::numeric_limits<double>::quiet_NaN();
    return state;
}

void applyDefaultProvenance(EphemerisResultMetadata& metadata)
{
    if (metadata.dataSourceProvenance.empty()) {
        metadata.dataSourceProvenance = kHighPrecisionDataSourceProvenance;
    }
}

void normalizeStatusForAvailableFallback(EphemerisResultMetadata& metadata)
{
    if (metadata.status == EphemerisResultStatus::OutOfRange) {
        metadata.status = EphemerisResultStatus::Degraded;
        metadata.addWarning(EphemerisWarningCode::DataOutOfRange);
    }
}

void normalizeMissingCoordinateStatus(EphemerisResultMetadata& metadata)
{
    switch (metadata.status) {
    case EphemerisResultStatus::Valid:
    case EphemerisResultStatus::Degraded:
        metadata.status = EphemerisResultStatus::Failed;
        metadata.addWarning(EphemerisWarningCode::ComputationFailed);
        break;
    case EphemerisResultStatus::OutOfRange:
        metadata.addWarning(EphemerisWarningCode::DataOutOfRange);
        break;
    case EphemerisResultStatus::Unsupported:
        metadata.addWarning(EphemerisWarningCode::UnsupportedBody);
        break;
    case EphemerisResultStatus::Failed:
        metadata.addWarning(EphemerisWarningCode::ComputationFailed);
        break;
    }
}

}  // namespace

CelestialBodyState EphemerisResultBuilder::buildState(
    const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult
) const
{
    CelestialBodyState state = makeEmptyState(input.bodyIndex);
    state.metadata = calculatorResult.metadata;
    applyDefaultProvenance(state.metadata);
    state.metadata.finalizeCorrectionTracking(input.request.options.correctionFlags);

    if (calculatorResult.equatorial.has_value()) {
        state.equatorial = *calculatorResult.equatorial;
        normalizeStatusForAvailableFallback(state.metadata);
    } else {
        normalizeMissingCoordinateStatus(state.metadata);
    }

    if (calculatorResult.horizontal.has_value()) {
        state.horizontal = *calculatorResult.horizontal;
    }

    return state;
}

CelestialBodyState EphemerisResultBuilder::buildUnsupportedState(const HighPrecisionComputationInput& input) const
{
    CelestialBodyState state = makeEmptyState(input.bodyIndex);
    state.metadata.status = EphemerisResultStatus::Unsupported;
    state.metadata.addWarning(EphemerisWarningCode::UnsupportedBody);
    state.metadata.dataSourceProvenance = kHighPrecisionDataSourceProvenance;
    state.metadata.finalizeCorrectionTracking(input.request.options.correctionFlags);
    return state;
}

CelestialBodyState EphemerisResultBuilder::buildFailedState(const HighPrecisionComputationInput& input) const
{
    CelestialBodyState state = makeEmptyState(input.bodyIndex);
    state.metadata.status = EphemerisResultStatus::Failed;
    state.metadata.addWarning(EphemerisWarningCode::ComputationFailed);
    state.metadata.dataSourceProvenance = kHighPrecisionDataSourceProvenance;
    state.metadata.finalizeCorrectionTracking(input.request.options.correctionFlags);
    return state;
}

}  // namespace skygate::ephemeris::highprecision
