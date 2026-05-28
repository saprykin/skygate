#pragma once

#include "EphemerisEngineFactoryRequest.hpp"
#include "EphemerisEngineFactoryResult.hpp"
#include "EphemerisFactoryCreationDiagnosticCode.hpp"
#include "EphemerisFactoryCreationStatus.hpp"
#include "EphemerisFactoryFallbackPolicy.hpp"
#include "catalog/IStarCatalog.hpp"

#include <initializer_list>
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
    [[nodiscard]] static EphemerisEngineFactoryResult create(std::initializer_list<OwnGalaxyCelestialBody> bodies);
    [[nodiscard]] static EphemerisEngineFactoryResult create(std::span<const OwnGalaxyCelestialBody> bodies);
    [[nodiscard]] static EphemerisEngineFactoryResult create(const CelestialBodyCatalog& catalog);

    EphemerisEngineFactory() = delete;
};

}  // namespace skygate::ephemeris
