#pragma once

#include "skygate/core/Types.hpp"
#include "skygate/ephemeris/Types.hpp"

namespace skygate::ephemeris {

class IEphemerisEngine {
public:
    virtual ~IEphemerisEngine() = default;

    [[nodiscard]] virtual SkySnapshot compute(const core::SkyContext& context) const = 0;
};

}  // namespace skygate::ephemeris
