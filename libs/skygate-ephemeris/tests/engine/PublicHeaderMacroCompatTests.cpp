#define None 0L

#include "skygate/ephemeris/CatalogLoadResult.hpp"
#include "skygate/ephemeris/CatalogLoader.hpp"
#include "skygate/ephemeris/IEphemerisEngine.hpp"

#include <cstdint>

int verifyPublicHeaderLegacyAliases();

int main()
{
    static_assert(None == 0L);
    static_assert(static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::NoCorrections) == 0U);
    static_assert(static_cast<std::uint8_t>(skygate::ephemeris::CatalogSelectionMode::Disabled) == 0U);
    static_assert(static_cast<std::uint8_t>(skygate::ephemeris::CatalogLoadErrorCode::NoError) == 0U);

    skygate::ephemeris::EphemerisCapabilities capabilities;
    return static_cast<int>(capabilities.supportedCorrections) + verifyPublicHeaderLegacyAliases();
}
