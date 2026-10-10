#pragma once

#include "engine/highprecision/IFrameTransformer.hpp"

#include <vector>

namespace skygate::ephemeris::tests {

// Returns one result fewer than requested to model an incomplete frame
// transform batch.
class ShortBatchFrameTransformer final : public skygate::ephemeris::highprecision::IFrameTransformer {
public:
    [[nodiscard]] std::vector<skygate::ephemeris::highprecision::CelestialFrameTransformResult>
    transform(const skygate::ephemeris::highprecision::CelestialFrameTransformRequest& request) const override;

    [[nodiscard]] int batchCallCount() const noexcept;

private:
    mutable int m_batchCallCount = 0;
};

}  // namespace skygate::ephemeris::tests
