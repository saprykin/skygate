#include "EphemerisEngineOptions.hpp"

#include <bit>
#include <cstdint>

namespace skygate::ephemeris {

bool EphemerisEngineOptions::isOptionSet(const Key key) const noexcept
{
    return (m_specifiedMask & BitFlagSet<Key>::bit(key)) != 0U;
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

bool EphemerisEngineOptions::isIdenticalTo(const EphemerisEngineOptions& other) const noexcept
{
    return engineKind() == other.engineKind() && correctionFlags() == other.correctionFlags()
           && fallbackToSimpleEngine() == other.fallbackToSimpleEngine()
           && enableAtmosphericRefraction() == other.enableAtmosphericRefraction()
           && std::bit_cast<std::uint64_t>(atmosphericPressureHpa())
                  == std::bit_cast<std::uint64_t>(other.atmosphericPressureHpa())
           && std::bit_cast<std::uint64_t>(atmosphericTemperatureC())
                  == std::bit_cast<std::uint64_t>(other.atmosphericTemperatureC())
           && std::bit_cast<std::uint64_t>(relativeHumidity()) == std::bit_cast<std::uint64_t>(other.relativeHumidity())
           && std::bit_cast<std::uint64_t>(observingWavelengthMicrometers())
                  == std::bit_cast<std::uint64_t>(other.observingWavelengthMicrometers());
}

}  // namespace skygate::ephemeris
