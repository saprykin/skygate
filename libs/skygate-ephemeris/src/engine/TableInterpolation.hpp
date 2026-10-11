#pragma once

#include <optional>

namespace skygate::ephemeris {

class TableInterpolation final {
public:
    TableInterpolation() = delete;

    // Returns the position of 'value' between 'lowerKey' and 'upperKey' as a
    // ratio. Returns nullopt when the interval has no positive width.
    [[nodiscard]] static std::optional<double> ratio(double value, double lowerKey, double upperKey) noexcept;
};

}  // namespace skygate::ephemeris
