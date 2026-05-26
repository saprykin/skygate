#pragma once

#include "SkyContext.hpp"
#include "Types.hpp"
#include "UtcTimePoint.hpp"

namespace skygate::ephemeris {

class EphemerisRequestFactory final {
public:
    [[nodiscard]] static core::SkyContext contextFromRequest(const EphemerisRequest& request) noexcept;
    [[nodiscard]] static EphemerisRequest
    requestFromContext(const core::SkyContext& context, const EphemerisEngineOptions& options) noexcept;
    [[nodiscard]] static EphemerisRequest
    atUtcTime(const EphemerisRequest& baseRequest, const core::UtcTimePoint& utcTime) noexcept;
};

}  // namespace skygate::ephemeris
