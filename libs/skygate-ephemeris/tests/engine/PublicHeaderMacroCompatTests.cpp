#define None 0L

#include "skygate/ephemeris/IEphemerisEngine.hpp"

#include <cstdint>

int main()
{
    static_assert(None == 0L);
    static_assert(static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::NoCorrections) == 0U);

    skygate::ephemeris::EphemerisCapabilities capabilities;
    return static_cast<int>(capabilities.supportedCorrections);
}
