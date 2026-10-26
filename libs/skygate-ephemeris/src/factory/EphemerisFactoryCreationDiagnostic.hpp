#pragma once

#include "EphemerisFactoryCreationDiagnosticSeverity.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace skygate::ephemeris {

struct EphemerisFactoryCreationDiagnostic {
    enum class Code : std::uint8_t {
        HighPrecisionUnavailable,
        RequiredEphemerisDataUnavailable,
        RequiredTimeScaleServiceUnavailable,
        RequiredEarthOrientationProviderUnavailable,
        InvalidRequest,
        EngineCreationFailed
    };

    Code code = Code::EngineCreationFailed;
    EphemerisFactoryCreationDiagnosticSeverity severity = EphemerisFactoryCreationDiagnosticSeverity::Error;
    std::string diagnosticText;

    EphemerisFactoryCreationDiagnostic() = default;

    EphemerisFactoryCreationDiagnostic(
        const Code diagnosticCode,
        const EphemerisFactoryCreationDiagnosticSeverity diagnosticSeverity,
        std::string diagnosticText = {}
    );

    [[nodiscard]] std::string_view displayText() const noexcept;

    [[nodiscard]] static std::string_view text(Code code) noexcept;

    [[nodiscard]] bool isError() const noexcept;
};

}  // namespace skygate::ephemeris
