#pragma once

#include "engine/highprecision/IStarAstrometryCalculator.hpp"

#include <cstddef>
#include <memory>
#include <vector>

namespace skygate::ui::tests {

// Astrometry double shared by the performance guard suites: it returns
// synthetic valid results for every requested body and records single and
// batch calls so the suites can assert the engine dispatch shape.
class GuardBatchStarAstrometryCalculator final : public skygate::ephemeris::highprecision::IStarAstrometryCalculator {
public:
    [[nodiscard]] skygate::ephemeris::highprecision::HighPrecisionCalculatorResult
    calculate(const skygate::ephemeris::highprecision::HighPrecisionComputationInput& input) const override;

    [[nodiscard]] std::vector<skygate::ephemeris::highprecision::StarAstrometryBatchResult> calculateBatch(
        const skygate::ephemeris::EphemerisRequest& request,
        const skygate::ephemeris::highprecision::CatalogStarAstrometryArrays& arrays,
        std::shared_ptr<const skygate::ephemeris::highprecision::PreparedEphemerisRequestState> preparedRequestState =
            {}
    ) const override;

    [[nodiscard]] int singleCallCount() const noexcept;
    [[nodiscard]] int batchCallCount() const noexcept;
    [[nodiscard]] std::size_t lastBatchSize() const noexcept;

private:
    mutable int m_singleCallCount = 0;
    mutable int m_batchCallCount = 0;
    mutable std::size_t m_lastBatchSize = 0U;
};

}  // namespace skygate::ui::tests
