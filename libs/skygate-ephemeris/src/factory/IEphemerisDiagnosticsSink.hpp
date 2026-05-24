#pragma once

namespace skygate::ephemeris {

struct EphemerisFactoryCreationDiagnostic;

class IEphemerisDiagnosticsSink {
public:
    virtual ~IEphemerisDiagnosticsSink() = default;

    virtual void recordFactoryCreationDiagnostic(const EphemerisFactoryCreationDiagnostic& diagnostic) = 0;
};

}  // namespace skygate::ephemeris
