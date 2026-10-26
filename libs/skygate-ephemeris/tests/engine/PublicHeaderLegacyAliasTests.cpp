#include "catalog/CatalogLoadResult.hpp"

#include <cstdint>

#ifdef None
#error "PublicHeaderLegacyAliasTests.cpp must compile without an X11-style None macro"
#endif

static_assert(static_cast<std::uint8_t>(skygate::ephemeris::CatalogLoadResult::ErrorCode::NoError) == 0U);
