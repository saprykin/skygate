#pragma once

#include "EphemerisRequest.hpp"
#include "ObservationContext.hpp"
#include "UtcTimePoint.hpp"
#include "engine/EphemerisEngineOptions.hpp"

namespace skygate::ephemeris {

class EphemerisRequestFactory final {
public:
    [[nodiscard]] static skygate::core::ObservationContext contextFromRequest(const EphemerisRequest& request) noexcept;
    [[nodiscard]] static EphemerisRequest requestFromContext(
        const skygate::core::ObservationContext& context, const EphemerisEngineOptions& options
    ) noexcept;
    [[nodiscard]] static EphemerisRequest
    atUtcTime(const EphemerisRequest& baseRequest, const skygate::core::UtcTimePoint& utcTime) noexcept;
};

}  // namespace skygate::ephemeris
