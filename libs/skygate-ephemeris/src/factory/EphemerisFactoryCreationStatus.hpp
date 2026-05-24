#pragma once

#include <cstdint>

namespace skygate::ephemeris {

enum class EphemerisFactoryCreationStatus : std::uint8_t {
    CreatedRequestedEngine,
    CreatedSimpleFallback,
    FailedStrictHighPrecisionUnavailable,
    FailedInvalidRequest,
    FailedCreationError
};

}  // namespace skygate::ephemeris
