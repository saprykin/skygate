#pragma once

#include "engine/IEphemerisEngine.hpp"
#include "engine/highprecision/HighPrecisionTypes.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace skygate::ephemeris::highprecision {

class HighPrecisionEphemerisEngine final : public IEphemerisEngine {
public:
    HighPrecisionEphemerisEngine(
        std::span<const CelestialBody> bodies,
        EphemerisEngineOptions engineOptions,
        HighPrecisionEphemerisEngineDependencies dependencies
    );

    ~HighPrecisionEphemerisEngine() override;

    HighPrecisionEphemerisEngine(const HighPrecisionEphemerisEngine&) = delete;
    HighPrecisionEphemerisEngine& operator=(const HighPrecisionEphemerisEngine&) = delete;
    HighPrecisionEphemerisEngine(HighPrecisionEphemerisEngine&&) noexcept;
    HighPrecisionEphemerisEngine& operator=(HighPrecisionEphemerisEngine&&) noexcept;

    [[nodiscard]] EphemerisEngineKind::Type kind() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;
    [[nodiscard]] EphemerisCapabilities capabilities() const noexcept override;
    [[nodiscard]] std::span<const EphemerisDateRange> supportedDateRanges() const noexcept override;
    [[nodiscard]] EphemerisDatasetInfo dataSetInfo() const override;
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
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace skygate::ephemeris::highprecision
