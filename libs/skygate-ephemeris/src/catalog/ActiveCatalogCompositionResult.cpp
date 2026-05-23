#include "skygate/ephemeris/ActiveCatalogCompositionResult.hpp"

namespace skygate::ephemeris {

bool ActiveCatalogCompositionResult::isSuccess() const noexcept
{
    return catalog != nullptr;
}

}  // namespace skygate::ephemeris
