#pragma once

#include "EphemerisDataActivationRequest.hpp"
#include "EphemerisDataActivationResult.hpp"

namespace skygate::ephemeris {

class EphemerisDataActivation final {
public:
    [[nodiscard]] static EphemerisDataActivationResult activate(const EphemerisDataActivationRequest& request);
};

}  // namespace skygate::ephemeris
