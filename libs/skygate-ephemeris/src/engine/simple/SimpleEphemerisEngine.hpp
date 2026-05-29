#pragma once

#include "MoonEquatorialCalculator.hpp"
#include "PlanetEquatorialCalculator.hpp"
#include "SunEquatorialCalculator.hpp"
#include "engine/IEphemerisEngine.hpp"

#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace skygate::ephemeris {

[[nodiscard]] EphemerisEngineOptions simpleEphemerisEngineDefaultOptions() noexcept;

class SimpleEphemerisEngine final : public IEphemerisEngine {
public:
    explicit SimpleEphemerisEngine(
        const CelestialBodyCatalog& catalog,
        EphemerisEngineOptions engineOptions = simpleEphemerisEngineDefaultOptions()
    );

    [[nodiscard]] EphemerisEngineKind::Type kind() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;
    [[nodiscard]] EphemerisCapabilities capabilities() const noexcept override;
    [[nodiscard]] std::span<const EphemerisDateRange> supportedDateRanges() const noexcept override;
    [[nodiscard]] EphemerisDatasetInfo dataSetInfo() const override;
    [[nodiscard]] EphemerisEngineOptions options() const noexcept override;
    [[nodiscard]] EphemerisSnapshot compute(const EphemerisRequest& request) const override;
    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const EphemerisRequest& request, std::string_view bodyId) const override;
    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const EphemerisRequest& request, std::size_t bodyIndex) const override;
    [[nodiscard]] EphemerisSnapshot compute(const skygate::core::ObservationContext& context) const override;
    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext& context, std::string_view bodyId) const override;
    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext& context, std::uint32_t bodyIndex) const override;

private:
    [[nodiscard]] EphemerisSnapshot computeSnapshot(const skygate::core::ObservationContext& context) const;
    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyStateById(const skygate::core::ObservationContext& context, std::string_view bodyId) const;
    [[nodiscard]] CelestialBodyState computeStateForBody(
        const BaseCelestialBody& body, std::size_t bodyIndex, const skygate::core::ObservationContext& context
    ) const;
    [[nodiscard]] std::optional<skygate::core::EquatorialCoordinate>
    computeEquatorial(const BaseCelestialBody& body, const skygate::core::UtcTimePoint& utcTime) const;

    std::shared_ptr<const CelestialBodyCatalog> m_catalog;
    EphemerisEngineOptions m_options;
    SunEquatorialCalculator m_sunCalculator;
    MoonEquatorialCalculator m_moonCalculator;
    PlanetEquatorialCalculator m_planetCalculator;
};

}  // namespace skygate::ephemeris
