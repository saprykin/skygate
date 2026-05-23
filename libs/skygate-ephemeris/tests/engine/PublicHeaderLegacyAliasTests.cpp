#include "catalog/CatalogLoadResult.hpp"
#include "catalog/CatalogSelectionMode.hpp"

#include <cstdint>

#ifdef None
#error "PublicHeaderLegacyAliasTests.cpp must compile without an X11-style None macro"
#endif

static_assert(static_cast<std::uint8_t>(skygate::ephemeris::CatalogSelectionMode::Disabled) == 0U);
static_assert(static_cast<std::uint8_t>(skygate::ephemeris::CatalogLoadResult::ErrorCode::NoError) == 0U);

int verifyPublicHeaderLegacyAliases()
{
    const auto selectionMode = skygate::ephemeris::CatalogSelectionMode::Disabled;
    const auto errorCode = skygate::ephemeris::CatalogLoadResult::ErrorCode::NoError;

    return static_cast<int>(selectionMode) + static_cast<int>(errorCode);
}
