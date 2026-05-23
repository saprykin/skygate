#include "skygate/ephemeris/CatalogSelectionOptions.hpp"

namespace skygate::ephemeris {

bool CatalogSelectionOptions::isEnabled() const noexcept
{
    return mode != CatalogSelectionMode::Disabled && maxBodyCount > 0;
}

}  // namespace skygate::ephemeris
