#include "CatalogCompositionResult.hpp"

namespace skygate::ephemeris {

bool CatalogCompositionResult::isSuccess() const noexcept
{
    return catalog != nullptr;
}

}  // namespace skygate::ephemeris
