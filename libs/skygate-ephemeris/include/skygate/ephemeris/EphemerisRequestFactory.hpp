#pragma once

#include "skygate/core/SkyContext.hpp"
#include "skygate/core/UtcTimePoint.hpp"
#include "skygate/ephemeris/Types.hpp"

namespace skygate::ephemeris {

class EphemerisRequestFactory final {
public:
    [[nodiscard]] static bool hasExplicitEpoch(const AstronomicalEpoch& epoch) noexcept;
    [[nodiscard]] static AstronomicalEpoch epochFromUtcTime(const core::UtcTimePoint& utcTime) noexcept;
    [[nodiscard]] static core::UtcTimePoint utcTimeFromEpoch(const AstronomicalEpoch& epoch) noexcept;
    [[nodiscard]] static core::SkyContext contextFromRequest(const EphemerisRequest& request) noexcept;
    [[nodiscard]] static EphemerisRequest
    fromContext(const core::SkyContext& context, const EphemerisEngineOptions& options) noexcept;
    [[nodiscard]] static EphemerisRequest
    atUtcTime(const EphemerisRequest& baseRequest, const core::UtcTimePoint& utcTime) noexcept;
};

}  // namespace skygate::ephemeris
