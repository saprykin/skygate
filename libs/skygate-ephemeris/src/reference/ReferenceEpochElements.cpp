#include "ReferenceEpochElements.hpp"
#include "time/AstronomicalTime.hpp"
#include "time/EpochCodec.hpp"

namespace skygate::ephemeris {

double ReferenceEpochElements::daysSinceJ2000(const skygate::core::UtcTimePoint& utcTime) noexcept
{
    return skygate::core::EpochCodec::daysSinceJ2000(utcTime);
}

double ReferenceEpochElements::meanObliquityDeg(const double daysSinceJ2000) noexcept
{
    return skygate::core::AstronomicalTime::meanObliquityDeg(daysSinceJ2000);
}

}  // namespace skygate::ephemeris
