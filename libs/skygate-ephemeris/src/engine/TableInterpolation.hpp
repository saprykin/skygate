#pragma once

#include <optional>

namespace skygate::ephemeris {

class TableInterpolation final {
public:
    TableInterpolation() = delete;

    // Returns the position of 'value' between 'lowerKey' and 'upperKey' as a
    // ratio. Returns nullopt when the interval has no positive width.
    [[nodiscard]] static std::optional<double> ratio(double value, double lowerKey, double upperKey) noexcept;

    // Returns the position of 'value' between 'lowerKey' and 'upperKey' as a
    // ratio clamped to the interval, so a bracket that overshoots still maps
    // onto its endpoints. Returns nullopt when the interval has no width.
    [[nodiscard]] static std::optional<double> clampedRatio(double value, double lowerKey, double upperKey) noexcept;

    // Returns the linear interpolation between the two samples of an interval
    // at a ratio that ratio() reported for that interval.
    [[nodiscard]] static double lerp(double lower, double upper, double ratio) noexcept;
};

}  // namespace skygate::ephemeris
