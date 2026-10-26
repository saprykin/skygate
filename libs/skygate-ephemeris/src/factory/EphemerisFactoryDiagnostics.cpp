#include "EphemerisFactoryDiagnostics.hpp"
#include "IEphemerisDiagnosticsSink.hpp"
#include "engine/EphemerisLogging.hpp"

#include <QDebug>

#include <string>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

EphemerisFactoryCreationDiagnostic EphemerisFactoryDiagnostics::creationDiagnostic(
    const EphemerisFactoryCreationDiagnostic::Code code,
    const EphemerisFactoryCreationDiagnosticSeverity severity,
    std::string text
)
{
    return {code, severity, std::move(text)};
}

std::vector<EphemerisFactoryCreationDiagnostic> EphemerisFactoryDiagnostics::withSeverity(
    std::vector<EphemerisFactoryCreationDiagnostic> diagnostics,
    const EphemerisFactoryCreationDiagnosticSeverity severity
)
{
    for (EphemerisFactoryCreationDiagnostic& diagnostic : diagnostics) {
        diagnostic.severity = severity;
    }

    return diagnostics;
}

EphemerisFactoryCreationStatus::Type
EphemerisFactoryDiagnostics::failureStatus(const std::vector<EphemerisFactoryCreationDiagnostic>& diagnostics) noexcept
{
    for (const EphemerisFactoryCreationDiagnostic& diagnostic : diagnostics) {
        if (diagnostic.code == EphemerisFactoryCreationDiagnostic::Code::EngineCreationFailed
            || diagnostic.code == EphemerisFactoryCreationDiagnostic::Code::InvalidRequest) {
            return EphemerisFactoryCreationStatus::Type::FailedCreationError;
        }
    }

    return EphemerisFactoryCreationStatus::Type::FailedStrictHighPrecisionUnavailable;
}

void EphemerisFactoryDiagnostics::publish(
    IEphemerisDiagnosticsSink* diagnosticsSink, const std::vector<EphemerisFactoryCreationDiagnostic>& diagnostics
)
{
    if (diagnosticsSink == nullptr) {
        return;
    }

    for (const EphemerisFactoryCreationDiagnostic& diagnostic : diagnostics) {
        diagnosticsSink->recordFactoryCreationDiagnostic(diagnostic);
    }
}

void EphemerisFactoryDiagnostics::log(const std::vector<EphemerisFactoryCreationDiagnostic>& diagnostics)
{
    for (const EphemerisFactoryCreationDiagnostic& diagnostic : diagnostics) {
        const QString text = QString::fromStdString(std::string{diagnostic.displayText()});
        if (diagnostic.isError()) {
            qCWarning(skygateEphemerisEngineLog).noquote() << text;
        } else {
            qCInfo(skygateEphemerisEngineLog).noquote() << text;
        }
    }
}

}  // namespace skygate::ephemeris
