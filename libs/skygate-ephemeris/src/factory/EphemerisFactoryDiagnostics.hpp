#pragma once

#include "EphemerisFactoryCreationDiagnostic.hpp"
#include "EphemerisFactoryCreationDiagnosticSeverity.hpp"
#include "EphemerisFactoryCreationStatus.hpp"

#include <string>
#include <vector>

namespace skygate::ephemeris {

class IEphemerisDiagnosticsSink;

class EphemerisFactoryDiagnostics final {
public:
    EphemerisFactoryDiagnostics() = delete;

    [[nodiscard]] static EphemerisFactoryCreationDiagnostic creationDiagnostic(
        EphemerisFactoryCreationDiagnostic::Code code,
        EphemerisFactoryCreationDiagnosticSeverity severity,
        std::string text = {}
    );

    [[nodiscard]] static std::vector<EphemerisFactoryCreationDiagnostic> withSeverity(
        std::vector<EphemerisFactoryCreationDiagnostic> diagnostics, EphemerisFactoryCreationDiagnosticSeverity severity
    );

    [[nodiscard]] static EphemerisFactoryCreationStatus::Type
    failureStatus(const std::vector<EphemerisFactoryCreationDiagnostic>& diagnostics) noexcept;

    static void publish(
        IEphemerisDiagnosticsSink* diagnosticsSink, const std::vector<EphemerisFactoryCreationDiagnostic>& diagnostics
    );

    static void log(const std::vector<EphemerisFactoryCreationDiagnostic>& diagnostics);
};

}  // namespace skygate::ephemeris
