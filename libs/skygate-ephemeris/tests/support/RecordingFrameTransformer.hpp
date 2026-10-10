#pragma once

#include "engine/EphemerisEngineQueryResult.hpp"
#include "engine/EphemerisEngineQueryStatus.hpp"
#include "math/Vector3d.hpp"
#include "engine/highprecision/CelestialReferenceFrame.hpp"
#include "engine/highprecision/IFrameTransformer.hpp"

#include <optional>
#include <vector>

namespace skygate::ephemeris::tests {

// Records each transform call and optionally injects the vector and metadata
// returned for it.
class RecordingFrameTransformer final : public skygate::ephemeris::highprecision::IFrameTransformer {
public:
    [[nodiscard]] std::vector<skygate::ephemeris::highprecision::CelestialFrameTransformResult>
    transform(const skygate::ephemeris::highprecision::CelestialFrameTransformRequest& request) const override;

    void setResultVector(const skygate::core::Vector3d& vector) noexcept;
    void setResultMetadata(const skygate::ephemeris::EphemerisEngineQueryResult& metadata);

    [[nodiscard]] int callCount() const noexcept;
    [[nodiscard]] skygate::ephemeris::highprecision::CelestialReferenceFrame::Type lastSourceFrame() const noexcept;
    [[nodiscard]] skygate::ephemeris::highprecision::CelestialReferenceFrame::Type lastTargetFrame() const noexcept;

private:
    mutable int m_callCount = 0;
    mutable skygate::ephemeris::highprecision::CelestialReferenceFrame::Type m_lastSourceFrame =
        skygate::ephemeris::highprecision::CelestialReferenceFrame::Type::Icrs;
    mutable skygate::ephemeris::highprecision::CelestialReferenceFrame::Type m_lastTargetFrame =
        skygate::ephemeris::highprecision::CelestialReferenceFrame::Type::Icrs;
    std::optional<skygate::core::Vector3d> m_resultVector;
    skygate::ephemeris::EphemerisEngineQueryResult m_resultMetadata = {
        .status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid
    };
};

}  // namespace skygate::ephemeris::tests
