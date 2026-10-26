#pragma once

#include "TimeScale.hpp"

#include <optional>

namespace skygate::core {

// Validity and raw-value mapping for the time scale a Julian date carries.
// The raw value is the stored ordinal, so catalogs and in-memory epochs agree
// on which scale a number names.
class TimeScaleCodec final {
public:
    [[nodiscard]] static bool isKnown(TimeScale timeScale) noexcept;
    [[nodiscard]] static std::optional<TimeScale> fromRaw(int rawValue) noexcept;
    // Returns -1 when the scale is not one this codec recognizes.
    [[nodiscard]] static int toRaw(TimeScale timeScale) noexcept;
};

}  // namespace skygate::core
