#pragma once

#include "BaseCelestialBody.hpp"
#include "EphemerisRequest.hpp"
#include "engine/highprecision/IApparentPlaceCalculator.hpp"

#include <memory>
#include <span>
#include <vector>

namespace skygate::ephemeris::tests {

// Returns calculator results unchanged where the apparent-place stage is not
// the behavior under test.
class PassThroughApparentPlaceCalculator final : public skygate::ephemeris::highprecision::IApparentPlaceCalculator {
public:
    [[nodiscard]] static std::shared_ptr<skygate::ephemeris::highprecision::IApparentPlaceCalculator> create();

    [[nodiscard]] skygate::ephemeris::highprecision::HighPrecisionCalculatorResult apply(
        const skygate::ephemeris::highprecision::HighPrecisionComputationInput&,
        const skygate::ephemeris::highprecision::HighPrecisionCalculatorResult& calculatorResult
    ) const override;

    [[nodiscard]] std::vector<skygate::ephemeris::highprecision::StarAstrometryBatchResult> applyBatch(
        const EphemerisRequest&,
        std::span<const BaseCelestialBody* const>,
        std::span<const skygate::ephemeris::highprecision::StarAstrometryBatchResult> calculatorResults,
        std::shared_ptr<const skygate::ephemeris::highprecision::PreparedEphemerisRequestState> preparedRequestState =
            {}
    ) const override;
};

}  // namespace skygate::ephemeris::tests
