#include "UtcTimeCodec.hpp"

#include <chrono>

namespace skygate::core {

UtcTimePoint UtcTimeCodec::fromEpochMicros(const std::int64_t epochMicros) noexcept
{
    return UtcTimePoint(std::chrono::microseconds(epochMicros));
}

std::int64_t UtcTimeCodec::toEpochMicros(const UtcTimePoint& utcTime) noexcept
{
    return static_cast<std::int64_t>(utcTime.time_since_epoch().count());
}

UtcTimePoint UtcTimeCodec::fromEpochSeconds(const std::int64_t epochSeconds) noexcept
{
    return UtcTimePoint(std::chrono::seconds(epochSeconds));
}

std::int64_t UtcTimeCodec::toEpochSecondsFloor(const UtcTimePoint& utcTime) noexcept
{
    return static_cast<std::int64_t>(std::chrono::floor<std::chrono::seconds>(utcTime.time_since_epoch()).count());
}

double UtcTimeCodec::secondsSinceEpochDouble(const UtcTimePoint& utcTime) noexcept
{
    return std::chrono::duration<double>(utcTime.time_since_epoch()).count();
}

}  // namespace skygate::core
