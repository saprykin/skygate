#include "ScreenPoint.hpp"

#include <cmath>

namespace skygate::core {

bool ScreenPoint::isVisible() const noexcept
{
    return status == ProjectionStatus::Visible;
}

bool ScreenPoint::isFinite() const noexcept
{
    return std::isfinite(x) && std::isfinite(y);
}

}  // namespace skygate::core
