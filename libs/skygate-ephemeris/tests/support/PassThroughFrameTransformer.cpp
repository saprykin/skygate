#include "PassThroughFrameTransformer.hpp"

#include "ApparentPlaceTestFixture.hpp"

namespace skygate::ephemeris::tests {

using namespace skygate::ephemeris::highprecision;
using namespace skygate::core;

std::vector<CelestialFrameTransformResult>
PassThroughFrameTransformer::transform(const CelestialFrameTransformRequest& request) const
{
    std::vector<CelestialFrameTransformResult> results;
    results.reserve(request.vectors.size());
    for (const Vector3d& vector : request.vectors) {
        ++m_callCount;
        m_lastSourceFrame = request.sourceFrame;
        m_lastTargetFrame = request.targetFrame;
        m_lastVector = vector;

        results.push_back(
            ApparentPlaceTestFixture::frameTransformResult(request.sourceFrame, request.targetFrame, vector)
        );
    }
    return results;
}

int PassThroughFrameTransformer::callCount() const noexcept
{
    return m_callCount;
}

CelestialReferenceFrame::Type PassThroughFrameTransformer::lastSourceFrame() const noexcept
{
    return m_lastSourceFrame;
}

CelestialReferenceFrame::Type PassThroughFrameTransformer::lastTargetFrame() const noexcept
{
    return m_lastTargetFrame;
}

Vector3d PassThroughFrameTransformer::lastVector() const noexcept
{
    return m_lastVector;
}

}  // namespace skygate::ephemeris::tests
