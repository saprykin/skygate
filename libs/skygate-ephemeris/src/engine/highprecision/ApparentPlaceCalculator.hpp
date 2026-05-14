#pragma once

#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"

#include <memory>

namespace skygate::ephemeris::highprecision {

class ApparentPlaceCalculator final : public IApparentPlaceCalculator {
public:
    ApparentPlaceCalculator(
        std::shared_ptr<const IFrameTransformer> frameTransformer,
        std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService,
        std::shared_ptr<const skygate::ephemeris::IEarthOrientationProvider> earthOrientationProvider,
        std::shared_ptr<const IAtmosphericRefractionCalculator> atmosphericRefractionCalculator = {}
    );

    [[nodiscard]] HighPrecisionCalculatorResult apply(
        const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult
    ) const override;
    [[nodiscard]] std::vector<StarAstrometryBatchResult> applyBatch(
        const EphemerisRequest& request,
        std::span<const CelestialBody> bodies,
        std::span<const StarAstrometryBatchResult> calculatorResults
    ) const override;

private:
    std::shared_ptr<const IFrameTransformer> m_frameTransformer;
    std::shared_ptr<const skygate::ephemeris::ITimeScaleService> m_timeScaleService;
    std::shared_ptr<const skygate::ephemeris::IEarthOrientationProvider> m_earthOrientationProvider;
    std::shared_ptr<const IAtmosphericRefractionCalculator> m_atmosphericRefractionCalculator;
};

}  // namespace skygate::ephemeris::highprecision
