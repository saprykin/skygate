#include "ScreenPoint.hpp"

#include <cmath>

namespace skygate::core {

ScreenPoint ScreenPoint::culled() noexcept
{
    return {};
}

ScreenPoint ScreenPoint::invalidCoordinate() noexcept
{
    ScreenPoint point;
    point.status = ProjectionStatus::InvalidCoordinate;
    return point;
}

ScreenPoint ScreenPoint::invalidParameters() noexcept
{
    ScreenPoint point;
    point.status = ProjectionStatus::InvalidParameters;
    return point;
}

bool ScreenPoint::isVisible() const noexcept
{
    return status == ProjectionStatus::Visible;
}

bool ScreenPoint::isFinite() const noexcept
{
    return std::isfinite(x) && std::isfinite(y);
}

}  // namespace skygate::core
