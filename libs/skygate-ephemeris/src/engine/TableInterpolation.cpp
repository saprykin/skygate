#include "TableInterpolation.hpp"

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

}  // namespace skygate::ephemeris
