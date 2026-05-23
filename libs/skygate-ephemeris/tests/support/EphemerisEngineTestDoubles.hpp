#pragma once

#include "IEphemerisEngine.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris::tests {

struct FixedAltitudeBody final {
    CelestialBody body;
    double altitudeDeg = 0.0;
    double azimuthDeg = 180.0;
};

[[nodiscard]] CelestialBody
makeFixedAltitudeBody(std::string id, double rightAscensionHours = 0.0, double declinationDeg = 0.0);

class FixedAltitudeEngine final : public IEphemerisEngine {
public:
    explicit FixedAltitudeEngine(double altitudeDeg);
    explicit FixedAltitudeEngine(std::vector<FixedAltitudeBody> bodies);

    [[nodiscard]] SkySnapshot compute(const EphemerisRequest& request) const override;

    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const EphemerisRequest& request, std::string_view bodyId) const override;

    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const EphemerisRequest& request, std::size_t bodyIndex) const override;

    [[nodiscard]] SkySnapshot compute(const skygate::core::SkyContext& context) const override;

    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext& context, std::string_view bodyId) const override;

    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext& context, std::uint32_t bodyIndex) const override;

private:
    [[nodiscard]] CelestialBodyState stateFor(std::uint32_t bodyIndex) const;

    std::shared_ptr<const std::vector<FixedAltitudeBody>> m_bodies;
    std::shared_ptr<const std::vector<CelestialBody>> m_catalogBodies;
};

class RequestCountingEphemerisEngine final : public IEphemerisEngine {
public:
    explicit RequestCountingEphemerisEngine(
        std::unique_ptr<IEphemerisEngine> engine,
        EphemerisEngineOptions options = {},
        std::shared_ptr<const std::vector<CelestialBody>> catalogBodies = {}
    );

    [[nodiscard]] EphemerisEngineKind kind() const noexcept override;
    [[nodiscard]] EphemerisEngineOptions options() const noexcept override;
    [[nodiscard]] SkySnapshot compute(const EphemerisRequest& request) const override;

    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const EphemerisRequest& request, std::string_view bodyId) const override;

    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const EphemerisRequest& request, std::size_t bodyIndex) const override;

    [[nodiscard]] SkySnapshot compute(const skygate::core::SkyContext& context) const override;

    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext& context, std::string_view bodyId) const override;

    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext& context, std::uint32_t bodyIndex) const override;

    [[nodiscard]] int requestSampleCount() const noexcept;
    [[nodiscard]] int contextSampleCount() const noexcept;

private:
    void attachCatalogBodies(SkySnapshot& snapshot) const;

    std::unique_ptr<IEphemerisEngine> m_engine;
    EphemerisEngineOptions m_options;
    std::shared_ptr<const std::vector<CelestialBody>> m_catalogBodies;
    mutable int m_requestSampleCount = 0;
    mutable int m_contextSampleCount = 0;
};

[[nodiscard]] EphemerisEngineOptions highPrecisionLightTimeOptions() noexcept;

}  // namespace skygate::ephemeris::tests
