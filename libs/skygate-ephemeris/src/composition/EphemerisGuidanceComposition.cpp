#include "EphemerisGuidanceComposition.hpp"
#include "engine/SimpleEphemerisGuidanceStrategy.hpp"

#include <memory>

namespace skygate::ephemeris {

std::shared_ptr<IEphemerisGuidanceStrategy> EphemerisGuidanceComposition::defaultGuidanceStrategy()
{
    return std::make_shared<SimpleEphemerisGuidanceStrategy>();
}

}  // namespace skygate::ephemeris
