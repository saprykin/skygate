#include "ShortBatchFrameTransformer.hpp"

#include "ApparentPlaceTestFixture.hpp"

#include <cstddef>

namespace skygate::ephemeris::tests {

using namespace skygate::ephemeris::highprecision;
using namespace skygate::core;

std::vector<CelestialFrameTransformResult>
ShortBatchFrameTransformer::transform(const CelestialFrameTransformRequest& request) const
{
    ++m_batchCallCount;
    std::vector<CelestialFrameTransformResult> results;
    const std::size_t resultCount = request.vectors.empty() ? 0U : request.vectors.size() - 1U;
    results.reserve(resultCount);
    for (std::size_t index = 0U; index < resultCount; ++index) {
        results.push_back(
            ApparentPlaceTestFixture::frameTransformResult(
                request.sourceFrame, request.targetFrame, request.vectors[index]
            )
        );
    }
    return results;
}

int ShortBatchFrameTransformer::batchCallCount() const noexcept
{
    return m_batchCallCount;
}

}  // namespace skygate::ephemeris::tests
