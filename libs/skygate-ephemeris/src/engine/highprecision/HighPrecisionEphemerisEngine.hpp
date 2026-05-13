#pragma once

#include "skygate/ephemeris/IEphemerisEngine.hpp"
#include "skygate/ephemeris/TimeScaleService.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace skygate::ephemeris::highprecision {

struct SolarSystemKernelVector {
    double xAu = 0.0;
    double yAu = 0.0;
    double zAu = 0.0;
};

struct SolarSystemKernelStateResult {
    std::optional<SolarSystemKernelVector> positionAu;
    std::optional<SolarSystemKernelVector> velocityAuPerDay;
    EphemerisResultMetadata metadata;
};

class ICalcephKernelProvider {
public:
    virtual ~ICalcephKernelProvider() = default;

    [[nodiscard]] virtual SolarSystemKernelStateResult
    computeGeometricState(const AstronomicalEpoch& epoch, int targetNaifId, int centerNaifId) const
    {
        static_cast<void>(epoch);
        static_cast<void>(targetNaifId);
        static_cast<void>(centerNaifId);

        SolarSystemKernelStateResult result;
        result.metadata.status = EphemerisResultStatus::Failed;
        result.metadata.addWarning(EphemerisWarningCode::MissingEphemerisData);
        return result;
    }
};

class IFrameTransformer;

class IAtmosphericRefractionCalculator {
public:
    virtual ~IAtmosphericRefractionCalculator() = default;
};

class IEphemerisComputationCache {
public:
    virtual ~IEphemerisComputationCache() = default;
};

struct HighPrecisionComputationInput {
    const EphemerisRequest& request;
    const CelestialBody& body;
    std::size_t bodyIndex = 0U;
};

struct HighPrecisionCalculatorResult {
    std::optional<core::EquatorialCoordinate> equatorial;
    std::optional<core::HorizontalCoordinate> horizontal;
    EphemerisResultMetadata metadata;
};

class ISolarSystemStateCalculator {
public:
    virtual ~ISolarSystemStateCalculator() = default;

    [[nodiscard]] virtual HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const = 0;
};

class IStarAstrometryCalculator {
public:
    virtual ~IStarAstrometryCalculator() = default;

    [[nodiscard]] virtual HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const = 0;
};

class IApparentPlaceCalculator {
public:
    virtual ~IApparentPlaceCalculator() = default;

    [[nodiscard]] virtual HighPrecisionCalculatorResult
    apply(const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult) const = 0;
};

class IEphemerisResultBuilder {
public:
    virtual ~IEphemerisResultBuilder() = default;

    [[nodiscard]] virtual CelestialBodyState buildState(
        const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult
    ) const = 0;

    [[nodiscard]] virtual CelestialBodyState buildUnsupportedState(const HighPrecisionComputationInput& input
    ) const = 0;
    [[nodiscard]] virtual CelestialBodyState buildFailedState(const HighPrecisionComputationInput& input) const = 0;
};

struct HighPrecisionEphemerisEngineDependencies {
    std::shared_ptr<const ICalcephKernelProvider> calcephKernelProvider;
    std::shared_ptr<const ISolarSystemStateCalculator> solarSystemStateCalculator;
    std::shared_ptr<const IStarAstrometryCalculator> starAstrometryCalculator;
    std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService;
    std::shared_ptr<const skygate::ephemeris::IEarthOrientationProvider> earthOrientationProvider;
    std::shared_ptr<const IFrameTransformer> frameTransformer;
    std::shared_ptr<const IApparentPlaceCalculator> apparentPlaceCalculator;
    std::shared_ptr<const IAtmosphericRefractionCalculator> atmosphericRefractionCalculator;
    std::shared_ptr<const IEphemerisResultBuilder> resultBuilder;
    std::shared_ptr<const IEphemerisComputationCache> computationCache;
    EphemerisDataSetInfo dataSetInfo;
};

class HighPrecisionEphemerisEngine final : public IEphemerisEngine {
public:
    HighPrecisionEphemerisEngine(
        std::span<const CelestialBody> bodies,
        EphemerisEngineOptions engineOptions,
        HighPrecisionEphemerisEngineDependencies dependencies
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
    [[nodiscard]] EphemerisRequest makeCompatibilityRequest(const core::SkyContext& context) const noexcept;
    [[nodiscard]] CelestialBodyState computeStateForBody(const EphemerisRequest& request, std::size_t bodyIndex) const;

    std::shared_ptr<const std::vector<CelestialBody>> m_bodies;
    EphemerisEngineOptions m_options;
    HighPrecisionEphemerisEngineDependencies m_dependencies;
};

}  // namespace skygate::ephemeris::highprecision
