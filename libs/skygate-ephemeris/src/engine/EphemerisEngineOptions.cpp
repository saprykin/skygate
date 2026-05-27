#include "engine/EphemerisEngineOptions.hpp"

namespace skygate::ephemeris {

bool EphemerisEngineOptions::isOptionSet(const Key key) const noexcept
{
    return (m_specifiedMask & maskBit(key)) != 0U;
}

bool EphemerisEngineOptions::operator==(const EphemerisEngineOptions& other) const noexcept
{
    return engineKind() == other.engineKind() && correctionFlags() == other.correctionFlags()
           && fallbackToSimpleEngine() == other.fallbackToSimpleEngine()
           && enableAtmosphericRefraction() == other.enableAtmosphericRefraction()
           && atmosphericPressureHpa() == other.atmosphericPressureHpa()
           && atmosphericTemperatureC() == other.atmosphericTemperatureC()
           && relativeHumidity() == other.relativeHumidity()
           && observingWavelengthMicrometers() == other.observingWavelengthMicrometers();
}

}  // namespace skygate::ephemeris
