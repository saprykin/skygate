#pragma once

#include "EphemerisFactoryCreationDiagnosticCode.hpp"
#include "EphemerisFactoryCreationDiagnosticSeverity.hpp"
#include "EphemerisFactoryCreationStatus.hpp"
#include "EphemerisFactoryFallbackPolicy.hpp"
#include "EphemerisEngineFactoryRequest.hpp"
#include "EphemerisEngineFactoryResult.hpp"
#include "catalog/IStarCatalog.hpp"
#include "engine/IEphemerisEngine.hpp"

#include <initializer_list>
#include <memory>
#include <span>
#include <string_view>

namespace skygate::ephemeris {

class EphemerisEngineFactory final {
public:
    [[nodiscard]] static bool allowsSimpleEngineFallback(EphemerisFactoryFallbackPolicy policy) noexcept;
    [[nodiscard]] static std::string_view displayName(EphemerisFactoryFallbackPolicy policy) noexcept;
    [[nodiscard]] static std::string_view displayName(EphemerisFactoryCreationStatus status) noexcept;
    [[nodiscard]] static bool isCreationSuccess(EphemerisFactoryCreationStatus status) noexcept;
    [[nodiscard]] static std::string_view diagnosticText(EphemerisFactoryCreationDiagnosticCode code) noexcept;

    [[nodiscard]] static EphemerisEngineFactoryResult create(const EphemerisEngineFactoryRequest& request);
    [[nodiscard]] static EphemerisEngineFactoryResult create();
    [[nodiscard]] static EphemerisEngineFactoryResult create(const IStarCatalog& catalog);
    [[nodiscard]] static EphemerisEngineFactoryResult create(std::initializer_list<CelestialBody> bodies);
    [[nodiscard]] static EphemerisEngineFactoryResult create(std::span<const CelestialBody> bodies);

    EphemerisEngineFactory() = delete;
};

}  // namespace skygate::ephemeris
