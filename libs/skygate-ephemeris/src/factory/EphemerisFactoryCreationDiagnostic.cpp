#include "EphemerisFactoryCreationDiagnostic.hpp"

namespace skygate::ephemeris {

EphemerisFactoryCreationDiagnostic::EphemerisFactoryCreationDiagnostic(
    const EphemerisFactoryCreationDiagnosticCode diagnosticCode,
    const EphemerisFactoryCreationDiagnosticSeverity diagnosticSeverity,
    std::string diagnosticText
)
    : code(diagnosticCode), severity(diagnosticSeverity), diagnosticText(std::move(diagnosticText))
{
}

std::string_view EphemerisFactoryCreationDiagnostic::displayText() const noexcept
{
    if (!diagnosticText.empty()) {
        return diagnosticText;
    }

    // Match the old ephemerisFactoryCreationDiagnosticText() mapping.
    switch (code) {
    case EphemerisFactoryCreationDiagnosticCode::HighPrecisionUnavailable:
        return "High-precision ephemeris creation is unavailable.";
    case EphemerisFactoryCreationDiagnosticCode::RequiredEphemerisDataUnavailable:
        return "Required ephemeris data is unavailable.";
    case EphemerisFactoryCreationDiagnosticCode::RequiredTimeScaleServiceUnavailable:
        return "Required time-scale service is unavailable.";
    case EphemerisFactoryCreationDiagnosticCode::RequiredEarthOrientationProviderUnavailable:
        return "Required Earth-orientation provider is unavailable.";
    case EphemerisFactoryCreationDiagnosticCode::InvalidRequest:
        return "The ephemeris engine factory request is invalid.";
    case EphemerisFactoryCreationDiagnosticCode::EngineCreationFailed:
        return "Ephemeris engine creation failed.";
    }

    return "Ephemeris engine creation diagnostic.";
}

bool EphemerisFactoryCreationDiagnostic::isError() const noexcept
{
    return severity == EphemerisFactoryCreationDiagnosticSeverity::Error;
}

}  // namespace skygate::ephemeris
