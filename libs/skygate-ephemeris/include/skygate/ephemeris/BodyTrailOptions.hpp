#pragma once

namespace skygate::ephemeris {

struct BodyTrailOptions final {
    int pastHours = 6;
    int futureHours = 18;
    int sampleStepMinutes = 30;
};

}  // namespace skygate::ephemeris
