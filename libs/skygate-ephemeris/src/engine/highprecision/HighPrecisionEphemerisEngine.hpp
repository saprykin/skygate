#pragma once

#include "engine/highprecision/CatalogStarAstrometryArrays.hpp"
#include "skygate/ephemeris/EarthOrientationProvider.hpp"
#include "skygate/ephemeris/IEphemerisEngine.hpp"
#include "skygate/ephemeris/TimeScaleService.hpp"

#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
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

struct PreparedEphemerisRequestState {
    EphemerisResultMetadata tdbKernelEpochMetadata;
    std::optional<AstronomicalEpoch> tdbKernelEpoch;
    std::optional<SolarSystemKernelStateResult> annualParallaxEarthState;

    bool topocentricStatePrepared = false;
    bool topocentricStateAvailable = true;
    EphemerisResultMetadata topocentricMetadata;
    std::optional<EarthOrientationSample> earthOrientationSample;
    std::optional<SolarSystemKernelVector> observerItrsPositionAu;
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

class IEphemerisComputationCache {
public:
    virtual ~IEphemerisComputationCache() = default;

    [[nodiscard]] virtual std::optional<SkySnapshot> findSnapshot(
        const EphemerisRequest& request,
        const std::vector<CelestialBody>& catalogBodies,
        const EphemerisDataSetInfo& dataSetInfo
    ) const
    {
        static_cast<void>(request);
        static_cast<void>(catalogBodies);
        static_cast<void>(dataSetInfo);
        return std::nullopt;
    }

    virtual void storeSnapshot(
        const EphemerisRequest& request,
        const std::vector<CelestialBody>& catalogBodies,
        const EphemerisDataSetInfo& dataSetInfo,
        const SkySnapshot& snapshot
    ) const
    {
        static_cast<void>(request);
        static_cast<void>(catalogBodies);
        static_cast<void>(dataSetInfo);
        static_cast<void>(snapshot);
    }

    [[nodiscard]] virtual std::shared_ptr<const PreparedEphemerisRequestState> findPreparedRequestState(
        const EphemerisRequest& request,
        const std::vector<CelestialBody>& catalogBodies,
        const EphemerisDataSetInfo& dataSetInfo
    ) const
    {
        static_cast<void>(request);
        static_cast<void>(catalogBodies);
        static_cast<void>(dataSetInfo);
        return nullptr;
    }

    virtual void storePreparedRequestState(
        const EphemerisRequest& request,
        const std::vector<CelestialBody>& catalogBodies,
        const EphemerisDataSetInfo& dataSetInfo,
        std::shared_ptr<const PreparedEphemerisRequestState> preparedState
    ) const
    {
        static_cast<void>(request);
        static_cast<void>(catalogBodies);
        static_cast<void>(dataSetInfo);
        static_cast<void>(preparedState);
    }

    [[nodiscard]] virtual std::optional<CelestialBodyState> findBodyState(
        const EphemerisRequest& request,
        const std::vector<CelestialBody>& catalogBodies,
        const EphemerisDataSetInfo& dataSetInfo,
        std::size_t bodyIndex
    ) const
    {
        static_cast<void>(request);
        static_cast<void>(catalogBodies);
        static_cast<void>(dataSetInfo);
        static_cast<void>(bodyIndex);
        return std::nullopt;
    }

    virtual void storeBodyState(
        const EphemerisRequest& request,
        const std::vector<CelestialBody>& catalogBodies,
        const EphemerisDataSetInfo& dataSetInfo,
        std::size_t bodyIndex,
        const CelestialBodyState& state
    ) const
    {
        static_cast<void>(request);
        static_cast<void>(catalogBodies);
        static_cast<void>(dataSetInfo);
        static_cast<void>(bodyIndex);
        static_cast<void>(state);
    }

    virtual void clear() const {}
};

struct HighPrecisionComputationInput {
    const EphemerisRequest& request;
    const CelestialBody& body;
    std::shared_ptr<const PreparedEphemerisRequestState> preparedRequestState;
    std::size_t bodyIndex = 0U;
};

struct HighPrecisionCalculatorResult {
    std::optional<core::EquatorialCoordinate> equatorial;
    std::optional<core::HorizontalCoordinate> horizontal;
    std::optional<SolarSystemKernelVector> observerRelativePositionAu;
    EphemerisResultMetadata metadata;
};

struct StarAstrometryBatchResult {
    std::size_t bodyIndex = 0U;
    HighPrecisionCalculatorResult result;
};

class IAtmosphericRefractionCalculator {
public:
    virtual ~IAtmosphericRefractionCalculator() = default;

    [[nodiscard]] virtual HighPrecisionCalculatorResult
    apply(const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult) const;
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
    [[nodiscard]] virtual std::vector<StarAstrometryBatchResult> calculateBatch(
        const EphemerisRequest& request,
        const CatalogStarAstrometryArrays& arrays,
        std::shared_ptr<const PreparedEphemerisRequestState> preparedRequestState = {}
    ) const
    {
        static_cast<void>(request);
        static_cast<void>(arrays);
        static_cast<void>(preparedRequestState);
        return {};
    }
};

class IApparentPlaceCalculator {
public:
    virtual ~IApparentPlaceCalculator() = default;

    [[nodiscard]] virtual HighPrecisionCalculatorResult
    apply(const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult) const = 0;
    [[nodiscard]] virtual std::vector<StarAstrometryBatchResult> applyBatch(
        const EphemerisRequest& request,
        std::span<const CelestialBody> bodies,
        std::span<const StarAstrometryBatchResult> calculatorResults,
        std::shared_ptr<const PreparedEphemerisRequestState> preparedRequestState = {}
    ) const
    {
        static_cast<void>(preparedRequestState);
        std::vector<StarAstrometryBatchResult> results;
        results.reserve(calculatorResults.size());
        for (const StarAstrometryBatchResult& calculatorResult : calculatorResults) {
            if (calculatorResult.bodyIndex >= bodies.size()) {
                continue;
            }
            const HighPrecisionComputationInput input{
                .request = request,
                .body = bodies[calculatorResult.bodyIndex],
                .preparedRequestState = preparedRequestState,
                .bodyIndex = calculatorResult.bodyIndex,
            };
            results.push_back(
                StarAstrometryBatchResult{
                    .bodyIndex = calculatorResult.bodyIndex,
                    .result = apply(input, calculatorResult.result),
                }
            );
        }
        return results;
    }
};

class IEphemerisResultBuilder {
public:
    virtual ~IEphemerisResultBuilder() = default;

    [[nodiscard]] virtual CelestialBodyState buildState(
        const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult
    ) const = 0;

    [[nodiscard]] virtual CelestialBodyState
    buildUnsupportedState(const HighPrecisionComputationInput& input) const = 0;
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
    struct DirectBodyStateCacheEntry {
        EphemerisRequest request;
        std::size_t bodyIndex = 0U;
        CelestialBodyState state;
    };

    [[nodiscard]] EphemerisRequest makeCompatibilityRequest(const core::SkyContext& context) const noexcept;
    [[nodiscard]] std::shared_ptr<const PreparedEphemerisRequestState>
    preparedRequestState(const EphemerisRequest& request) const;
    [[nodiscard]] std::shared_ptr<const PreparedEphemerisRequestState>
    buildPreparedRequestState(const EphemerisRequest& request) const;
    [[nodiscard]] SkySnapshot computeUncached(
        const EphemerisRequest& request, std::shared_ptr<const PreparedEphemerisRequestState> preparedState
    ) const;
    [[nodiscard]] CelestialBodyState computeStateForBody(
        const EphemerisRequest& request,
        std::size_t bodyIndex,
        std::shared_ptr<const PreparedEphemerisRequestState> preparedState
    ) const;
    [[nodiscard]] std::optional<CelestialBodyState>
    findDirectBodyState(const EphemerisRequest& request, std::size_t bodyIndex) const;
    void
    storeDirectBodyState(const EphemerisRequest& request, std::size_t bodyIndex, const CelestialBodyState& state) const;

    std::shared_ptr<const std::vector<CelestialBody>> m_bodies;
    CatalogStarAstrometryArrays m_catalogStarAstrometryArrays;
    EphemerisEngineOptions m_options;
    HighPrecisionEphemerisEngineDependencies m_dependencies;
    mutable std::mutex m_directBodyStateCacheMutex;
    mutable std::deque<DirectBodyStateCacheEntry> m_directBodyStateCache;
};

}  // namespace skygate::ephemeris::highprecision
