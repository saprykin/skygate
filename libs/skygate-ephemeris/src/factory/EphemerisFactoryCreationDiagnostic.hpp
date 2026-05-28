#pragma once

#include "EphemerisFactoryCreationDiagnosticCode.hpp"
#include "EphemerisFactoryCreationDiagnosticSeverity.hpp"

#include <string>
#include <string_view>

namespace skygate::ephemeris {

struct EphemerisFactoryCreationDiagnostic {
    EphemerisFactoryCreationDiagnosticCode code = EphemerisFactoryCreationDiagnosticCode::EngineCreationFailed;
    EphemerisFactoryCreationDiagnosticSeverity severity = EphemerisFactoryCreationDiagnosticSeverity::Error;
    std::string diagnosticText;

    EphemerisFactoryCreationDiagnostic() = default;

    EphemerisFactoryCreationDiagnostic(
        const EphemerisFactoryCreationDiagnosticCode diagnosticCode,
        const EphemerisFactoryCreationDiagnosticSeverity diagnosticSeverity,
        std::string diagnosticText = {}
    );

    [[nodiscard]] std::string_view displayText() const noexcept;

    [[nodiscard]] bool isError() const noexcept;
};

}  // namespace skygate::ephemeris
