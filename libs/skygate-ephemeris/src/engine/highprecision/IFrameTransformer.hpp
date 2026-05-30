#pragma once

#include "CelestialFrameTransformRequest.hpp"
#include "CelestialFrameTransformResult.hpp"

#include <vector>

namespace skygate::ephemeris::highprecision {

class IFrameTransformer {
public:
    virtual ~IFrameTransformer() = default;

    [[nodiscard]] virtual std::vector<CelestialFrameTransformResult>
    transform(const CelestialFrameTransformRequest& request) const = 0;
};

}  // namespace skygate::ephemeris::highprecision
