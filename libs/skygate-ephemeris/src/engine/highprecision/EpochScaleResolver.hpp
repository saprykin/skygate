#pragma once

#include "EphemerisMetadataFailurePolicy.hpp"
#include "time/AstronomicalEpoch.hpp"

#include <memory>
#include <optional>

namespace skygate::ephemeris {
class ITimeScaleService;
struct EphemerisEngineQueryResult;
}  // namespace skygate::ephemeris

namespace skygate::ephemeris::highprecision {

/// Resolves the TDB epoch a kernel query runs at.
///
/// A TDB request epoch needs no conversion. Any other scale is converted
/// through the time-scale service, and the conversion diagnostics reach the
/// query through the merger that keeps a TDB-approximation-only degradation
/// silent. The returned epoch is always normalized, and a resolution that
/// finds no service marks the query Failed or Degraded as the failure policy
/// asks and yields no epoch.
class EpochScaleResolver final {
public:
    EpochScaleResolver() = delete;

    [[nodiscard]] static std::optional<skygate::core::AstronomicalEpoch> resolve(
        EphemerisEngineQueryResult& metadata,
        const skygate::core::AstronomicalEpoch& epoch,
        const std::shared_ptr<const skygate::ephemeris::ITimeScaleService>& timeScaleService,
        EphemerisMetadataFailurePolicy failurePolicy
    ) noexcept;
};

}  // namespace skygate::ephemeris::highprecision
