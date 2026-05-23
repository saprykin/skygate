#include "CatalogBodyParseResult.hpp"

namespace skygate::ephemeris {

bool CatalogBodyParseResult::isSuccess() const noexcept
{
    return errorCode == CatalogLoadResult::ErrorCode::NoError;
}

}  // namespace skygate::ephemeris
