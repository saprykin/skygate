#pragma once

#include "BaseCelestialBody.hpp"
#include "EphemerisRequest.hpp"

#include <cstddef>
#include <memory>

namespace skygate::ephemeris::highprecision {

struct PreparedEphemerisRequestState;

struct HighPrecisionComputationInput {
    const EphemerisRequest& request;
    const BaseCelestialBody& body;
    std::shared_ptr<const PreparedEphemerisRequestState> preparedRequestState;
    std::size_t bodyIndex = 0U;
};

}  // namespace skygate::ephemeris::highprecision
