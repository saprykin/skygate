#pragma once

#include <cstdint>

namespace skygate::ephemeris {

enum class EphemerisFactoryCreationDiagnosticSeverity : std::uint8_t {
    Warning,
    Error
};

}  // namespace skygate::ephemeris
