#pragma once

#include "EphemerisEngineFactoryRequest.hpp"
#include "EphemerisEngineFactoryResult.hpp"

namespace skygate::ephemeris {

class HighPrecisionEngineAssembly final {
public:
    HighPrecisionEngineAssembly() = delete;

    /// Resolves the providers the request still lacks from its active data
    /// snapshot, then assembles the high-precision engine graph or the result
    /// the request fallback policy allows.
    [[nodiscard]] static EphemerisEngineFactoryResult assemble(EphemerisEngineFactoryRequest request);
};

}  // namespace skygate::ephemeris
