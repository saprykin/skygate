#include "TableInterpolation.hpp"

#include <algorithm>
#include <cmath>

namespace skygate::ephemeris {

std::optional<double>
TableInterpolation::ratio(const double value, const double lowerKey, const double upperKey) noexcept
{
    const double denominator = upperKey - lowerKey;
    if (!(denominator > 0.0)) {
        return std::nullopt;
    }

    return (value - lowerKey) / denominator;
}

std::optional<double>
TableInterpolation::clampedRatio(const double value, const double lowerKey, const double upperKey) noexcept
{
    const double denominator = upperKey - lowerKey;
    if (denominator == 0.0 || !std::isfinite(denominator)) {
        return std::nullopt;
    }

    return std::clamp((value - lowerKey) / denominator, 0.0, 1.0);
}

double TableInterpolation::lerp(const double lower, const double upper, const double ratio) noexcept
{
    return lower + (upper - lower) * ratio;
}

}  // namespace skygate::ephemeris
