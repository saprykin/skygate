#include "RecordingFrameTransformer.hpp"

namespace skygate::ephemeris::tests {

using namespace skygate::ephemeris::highprecision;
using namespace skygate::core;

std::vector<CelestialFrameTransformResult>
RecordingFrameTransformer::transform(const CelestialFrameTransformRequest& request) const
{
    std::vector<CelestialFrameTransformResult> results;
    results.reserve(request.vectors.size());
    for (const Vector3d& vector : request.vectors) {
        ++m_callCount;
        m_lastSourceFrame = request.sourceFrame;
        m_lastTargetFrame = request.targetFrame;
        results.push_back(
            CelestialFrameTransformResult{
                .vector = m_resultVector.value_or(vector),
                .metadata = m_resultMetadata,
            }
        );
    }
    return results;
}

void RecordingFrameTransformer::setResultVector(const Vector3d& vector) noexcept
{
    m_resultVector = vector;
}

void RecordingFrameTransformer::setResultMetadata(const EphemerisEngineQueryResult& metadata)
{
    m_resultMetadata = metadata;
}

int RecordingFrameTransformer::callCount() const noexcept
{
    return m_callCount;
}

CelestialReferenceFrame::Type RecordingFrameTransformer::lastSourceFrame() const noexcept
{
    return m_lastSourceFrame;
}

CelestialReferenceFrame::Type RecordingFrameTransformer::lastTargetFrame() const noexcept
{
    return m_lastTargetFrame;
}

}  // namespace skygate::ephemeris::tests
