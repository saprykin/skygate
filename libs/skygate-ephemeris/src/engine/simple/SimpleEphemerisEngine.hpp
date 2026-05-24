#pragma once

#include "engine/IEphemerisEngine.hpp"
#include "engine/simple/MoonEquatorialCalculator.hpp"
#include "engine/simple/PlanetEquatorialCalculator.hpp"
#include "engine/simple/SunEquatorialCalculator.hpp"

#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

[[nodiscard]] EphemerisEngineOptions simpleEphemerisEngineDefaultOptions() noexcept;

class SimpleEphemerisEngine final : public IEphemerisEngine {
public:
    explicit SimpleEphemerisEngine(
        std::span<const CelestialBody> bodies,
        EphemerisEngineOptions engineOptions = simpleEphemerisEngineDefaultOptions()
    );

    [[nodiscard]] EphemerisEngineKind kind() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;
    [[nodiscard]] EphemerisCapabilities capabilities() const noexcept override;
    [[nodiscard]] std::span<const EphemerisDateRange> supportedDateRanges() const noexcept override;
    [[nodiscard]] EphemerisDataSetInfo dataSetInfo() const override;
    [[nodiscard]] EphemerisEngineOptions options() const noexcept override;
    [[nodiscard]] SkySnapshot compute(const EphemerisRequest& request) const override;
    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const EphemerisRequest& request, std::string_view bodyId) const override;
    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const EphemerisRequest& request, std::size_t bodyIndex) const override;
    [[nodiscard]] SkySnapshot compute(const core::SkyContext& context) const override;
    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const core::SkyContext& context, std::string_view bodyId) const override;
    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const core::SkyContext& context, std::uint32_t bodyIndex) const override;

private:
    [[nodiscard]] SkySnapshot computeSnapshot(const core::SkyContext& context) const;
    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyStateById(const core::SkyContext& context, std::string_view bodyId) const;
    [[nodiscard]] CelestialBodyState
    computeStateForBody(const CelestialBody& body, std::size_t bodyIndex, const core::SkyContext& context) const;
    [[nodiscard]] std::optional<core::EquatorialCoordinate>
    computeEquatorial(const CelestialBody& body, const core::UtcTimePoint& utcTime) const;

    std::shared_ptr<const std::vector<CelestialBody>> m_bodies;
    EphemerisEngineOptions m_options;
    SunEquatorialCalculator m_sunCalculator;
    MoonEquatorialCalculator m_moonCalculator;
    PlanetEquatorialCalculator m_planetCalculator;
};

}  // namespace skygate::ephemeris
