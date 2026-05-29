#pragma once

#include "ObservationEventStatus.hpp"
#include "UtcTimePoint.hpp"

#include <optional>

namespace skygate::ephemeris {

struct ObservationEvent {
    ObservationEventStatus status = ObservationEventStatus::Unresolved;
    std::optional<skygate::core::UtcTimePoint> utcTime;
    std::optional<double> altitudeDeg;
};

}  // namespace skygate::ephemeris
