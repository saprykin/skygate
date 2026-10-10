#pragma once

#include "engine/highprecision/IFrameTransformer.hpp"

#include <vector>

namespace skygate::ephemeris::tests {

// Records each transform request and returns the input vectors unchanged with
// the metadata implied by the requested frame change.
class PassThroughFrameTransformer final : public skygate::ephemeris::highprecision::IFrameTransformer {
public:
    [[nodiscard]] std::vector<skygate::ephemeris::highprecision::CelestialFrameTransformResult>
    transform(const skygate::ephemeris::highprecision::CelestialFrameTransformRequest& request) const override;

    [[nodiscard]] int callCount() const noexcept;
    [[nodiscard]] skygate::ephemeris::highprecision::CelestialReferenceFrame::Type lastSourceFrame() const noexcept;
    [[nodiscard]] skygate::ephemeris::highprecision::CelestialReferenceFrame::Type lastTargetFrame() const noexcept;
    [[nodiscard]] skygate::core::Vector3d lastVector() const noexcept;

private:
    mutable int m_callCount = 0;
    mutable skygate::ephemeris::highprecision::CelestialReferenceFrame::Type m_lastSourceFrame =
        skygate::ephemeris::highprecision::CelestialReferenceFrame::Type::Icrs;
    mutable skygate::ephemeris::highprecision::CelestialReferenceFrame::Type m_lastTargetFrame =
        skygate::ephemeris::highprecision::CelestialReferenceFrame::Type::Icrs;
    mutable skygate::core::Vector3d m_lastVector;
};

}  // namespace skygate::ephemeris::tests
