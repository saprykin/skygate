#pragma once

#include "engine/EphemerisDatasetInfo.hpp"
#include "engine/IEphemerisEngine.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace skygate::ephemeris {
class ITimeScaleService;
class IEarthOrientationProvider;
class IEphemerisFallbackStrategy;
}  // namespace skygate::ephemeris

namespace skygate::ephemeris::highprecision {

// Forward declarations for interface classes referenced in dependencies.
class ICalcephKernel;
class ISolarSystemStateCalculator;
class IStarAstrometryCalculator;
class IApparentPlaceCalculator;
class IAtmosphericRefractionCalculator;
class IFrameTransformer;
class IEphemerisResultBuilder;
class IEphemerisComputationCache;

class HighPrecisionEphemerisEngine final : public IEphemerisEngine {
public:
    struct Dependencies {
        std::shared_ptr<const ICalcephKernel> calcephKernel;
        std::shared_ptr<const ISolarSystemStateCalculator> solarSystemStateCalculator;
        std::shared_ptr<const IStarAstrometryCalculator> starAstrometryCalculator;
        std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService;
        std::shared_ptr<const skygate::ephemeris::IEarthOrientationProvider> earthOrientationProvider;
        std::shared_ptr<const IFrameTransformer> frameTransformer;
        std::shared_ptr<const IApparentPlaceCalculator> apparentPlaceCalculator;
        std::shared_ptr<const IAtmosphericRefractionCalculator> atmosphericRefractionCalculator;
        std::shared_ptr<const IEphemerisResultBuilder> resultBuilder;
        std::shared_ptr<const IEphemerisComputationCache> computationCache;
        std::shared_ptr<const skygate::ephemeris::IEphemerisFallbackStrategy> fallbackStrategy;
        EphemerisDatasetInfo dataSetInfo;
    };

    HighPrecisionEphemerisEngine(
        const CelestialBodyCatalog& catalog, EphemerisEngineOptions engineOptions, Dependencies dependencies
    );

    ~HighPrecisionEphemerisEngine() override;

    HighPrecisionEphemerisEngine(const HighPrecisionEphemerisEngine&) = delete;
    HighPrecisionEphemerisEngine& operator=(const HighPrecisionEphemerisEngine&) = delete;
    HighPrecisionEphemerisEngine(HighPrecisionEphemerisEngine&&) noexcept;
    HighPrecisionEphemerisEngine& operator=(HighPrecisionEphemerisEngine&&) noexcept;

    [[nodiscard]] EphemerisEngineKind::Type kind() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;
    [[nodiscard]] EphemerisCapabilities capabilities() const noexcept override;
    [[nodiscard]] EphemerisEngineTraits traits() const noexcept override;
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
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace skygate::ephemeris::highprecision
