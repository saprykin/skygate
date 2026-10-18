#include "EphemerisFactoryCreationDiagnostic.hpp"

namespace skygate::ephemeris {

EphemerisFactoryCreationDiagnostic::EphemerisFactoryCreationDiagnostic(
    const Code diagnosticCode,
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

    return text(code);
}

std::string_view EphemerisFactoryCreationDiagnostic::text(const Code code) noexcept
{
    switch (code) {
    case Code::HighPrecisionUnavailable:
        return "High-precision ephemeris creation is unavailable.";
    case Code::RequiredEphemerisDataUnavailable:
        return "Required ephemeris data is unavailable.";
    case Code::RequiredTimeScaleServiceUnavailable:
        return "Required time-scale service is unavailable.";
    case Code::RequiredEarthOrientationProviderUnavailable:
        return "Required Earth-orientation provider is unavailable.";
    case Code::InvalidRequest:
        return "The ephemeris engine factory request is invalid.";
    case Code::EngineCreationFailed:
        return "Ephemeris engine creation failed.";
    }

    return "Ephemeris engine creation diagnostic.";
}

bool EphemerisFactoryCreationDiagnostic::isError() const noexcept
{
    return severity == EphemerisFactoryCreationDiagnosticSeverity::Error;
}

}  // namespace skygate::ephemeris
