#pragma once

#include <cstdint>

namespace skygate::ephemeris {

enum class EphemerisFactoryCreationDiagnosticCode : std::uint8_t {
    HighPrecisionUnavailable,
    RequiredEphemerisDataUnavailable,
    RequiredTimeScaleServiceUnavailable,
    RequiredEarthOrientationProviderUnavailable,
    InvalidRequest,
    EngineCreationFailed
};

}  // namespace skygate::ephemeris
