#include "skygate/core/ScreenPoint.hpp"

#include <cmath>

namespace skygate::core {

bool ScreenPoint::isFinite() const noexcept
{
    return std::isfinite(x) && std::isfinite(y);
}

}  // namespace skygate::core
