#pragma once

#include "ObservationEvent.hpp"

namespace skygate::ephemeris {

struct ObservationEventSummary {
    ObservationEvent nextRise;
    ObservationEvent nextSet;
    ObservationEvent culmination;
};

}  // namespace skygate::ephemeris
