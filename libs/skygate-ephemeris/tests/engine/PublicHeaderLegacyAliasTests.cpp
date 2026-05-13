#include "skygate/ephemeris/CatalogLoadResult.hpp"
#include "skygate/ephemeris/CatalogLoader.hpp"

#include <cstdint>

#ifdef None
#error "PublicHeaderLegacyAliasTests.cpp must compile without an X11-style None macro"
#endif

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

static_assert(
    static_cast<std::uint8_t>(skygate::ephemeris::CatalogSelectionMode::None)
    == static_cast<std::uint8_t>(skygate::ephemeris::CatalogSelectionMode::Disabled)
);
static_assert(
    static_cast<std::uint8_t>(skygate::ephemeris::CatalogLoadErrorCode::None)
    == static_cast<std::uint8_t>(skygate::ephemeris::CatalogLoadErrorCode::NoError)
);

int verifyPublicHeaderLegacyAliases()
{
    const auto selectionMode = skygate::ephemeris::CatalogSelectionMode::None;
    const auto errorCode = skygate::ephemeris::CatalogLoadErrorCode::None;

    return static_cast<int>(selectionMode) + static_cast<int>(errorCode);
}

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
