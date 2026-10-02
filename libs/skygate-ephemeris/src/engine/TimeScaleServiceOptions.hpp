#pragma once

#include "EarthOrientationSampler.hpp"

namespace skygate::ephemeris {

struct TimeScaleServiceOptions {
    bool allowDegradedLeapSecondFallback = false;
    int fallbackTaiMinusUtcSeconds = 0;
    EarthOrientationSampler::Options earthOrientationSampleOptions{false, false};
    bool allowUt1DeltaTFallback = false;
};

}  // namespace skygate::ephemeris
