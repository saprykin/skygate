#pragma once

#include "engine/highprecision/IApparentPlaceCalculator.hpp"

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace skygate::ui::tests {

// Apparent-place double shared by the performance guard suites: it applies the
// requested correction flags and synthetic horizontal coordinates to every
// result and records single and batch calls.
class GuardApparentPlaceCalculator final : public skygate::ephemeris::highprecision::IApparentPlaceCalculator {
public:
    [[nodiscard]] skygate::ephemeris::highprecision::HighPrecisionCalculatorResult apply(
        const skygate::ephemeris::highprecision::HighPrecisionComputationInput& input,
        const skygate::ephemeris::highprecision::HighPrecisionCalculatorResult& calculatorResult
    ) const override;

    [[nodiscard]] std::vector<skygate::ephemeris::highprecision::StarAstrometryBatchResult> applyBatch(
        const skygate::ephemeris::EphemerisRequest& request,
        std::span<const skygate::ephemeris::BaseCelestialBody* const> bodies,
        std::span<const skygate::ephemeris::highprecision::StarAstrometryBatchResult> calculatorResults,
        std::shared_ptr<const skygate::ephemeris::highprecision::PreparedEphemerisRequestState> preparedRequestState =
            {}
    ) const override;

    [[nodiscard]] int singleCallCount() const noexcept;
    [[nodiscard]] int batchCallCount() const noexcept;
    [[nodiscard]] std::size_t lastBatchSize() const noexcept;

private:
    [[nodiscard]] static skygate::core::HorizontalCoordinate horizontalForBodyIndex(std::size_t bodyIndex) noexcept;

    mutable int m_singleCallCount = 0;
    mutable int m_batchCallCount = 0;
    mutable std::size_t m_lastBatchSize = 0U;
};

}  // namespace skygate::ui::tests
