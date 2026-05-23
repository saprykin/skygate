#pragma once

#include "UtcTimePoint.hpp"

#include <cstdint>

namespace skygate::core {

class UtcTimeCodec final {
public:
    [[nodiscard]] static UtcTimePoint fromEpochMicros(std::int64_t epochMicros) noexcept;
    [[nodiscard]] static std::int64_t toEpochMicros(const UtcTimePoint& utcTime) noexcept;
    [[nodiscard]] static UtcTimePoint fromEpochSeconds(std::int64_t epochSeconds) noexcept;
    [[nodiscard]] static std::int64_t toEpochSecondsFloor(const UtcTimePoint& utcTime) noexcept;
    [[nodiscard]] static double secondsSinceEpochDouble(const UtcTimePoint& utcTime) noexcept;
};

}  // namespace skygate::core
