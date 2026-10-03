#pragma once

#include "EphemerisEngineKind.hpp"
#include "EphemerisEngineOptions.hpp"

#include <string>

namespace skygate::ephemeris {

struct EphemerisEngineDescriptor final {
    std::string id;
    EphemerisEngineKind::Type kind = EphemerisEngineKind::Type::Simple;
    std::string displayName;
    EphemerisEngineOptions defaultOptions;
    bool supportsCorrections = false;
    bool supportsAtmosphereSettings = false;
    bool requiresBackendResources = false;
};

}  // namespace skygate::ephemeris
