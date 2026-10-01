#pragma once

#include "CatalogStarAstrometryArrays.hpp"
#include "HighPrecisionCalculatorResult.hpp"
#include "HighPrecisionComputationInput.hpp"
#include "IStarAstrometryCalculator.hpp"
#include "StarAstrometryBatchResult.hpp"

#include <cstddef>
#include <vector>

namespace skygate::ephemeris {
class ITimeScaleService;
}  // namespace skygate::ephemeris

namespace skygate::ephemeris::highprecision {

struct PreparedEphemerisRequestState;

class ICalcephKernel;

class StarAstrometryCalculator final : public IStarAstrometryCalculator {
public:
    explicit StarAstrometryCalculator(
        std::shared_ptr<const ICalcephKernel> kernel = {},
        std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService = {}
    );

    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override;
    [[nodiscard]] std::vector<StarAstrometryBatchResult> calculateBatch(
        const EphemerisRequest& request,
        const CatalogStarAstrometryArrays& arrays,
        std::shared_ptr<const PreparedEphemerisRequestState> preparedRequestState = {}
    ) const override;

private:
    std::shared_ptr<const ICalcephKernel> m_kernel;
    std::shared_ptr<const skygate::ephemeris::ITimeScaleService> m_timeScaleService;
};

}  // namespace skygate::ephemeris::highprecision
