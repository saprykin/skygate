#pragma once

#include "SkyObjectTrailSamplingInput.hpp"
#include "trail/BodyTrailSample.hpp"

#include <vector>

// Rotates a sampled trail so that its present sample sits exactly on the
// selected object's current horizontal position.
class SkyObjectTrailAlignment final {
public:
    SkyObjectTrailAlignment() = delete;

    // Leaves the samples untouched until the selected state has a finite
    // horizontal position and the trail has a usable present sample.
    static void alignToSelectedState(
        std::vector<skygate::ephemeris::BodyTrailSample>& samples, const SkyObjectTrailSamplingInput& input
    );
};
