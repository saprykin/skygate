#include "TimeScaleCodec.hpp"

#include <algorithm>
#include <array>

namespace skygate::core {
namespace {

constexpr std::array<TimeScale, 5> kKnownValues{
    TimeScale::Utc,
    TimeScale::Tai,
    TimeScale::Tt,
    TimeScale::Tdb,
    TimeScale::Ut1,
};

constexpr int kUnknownRawValue = -1;

}  // namespace

bool TimeScaleCodec::isKnown(const TimeScale timeScale) noexcept
{
    return std::find(kKnownValues.begin(), kKnownValues.end(), timeScale) != kKnownValues.end();
}

std::optional<TimeScale> TimeScaleCodec::fromRaw(const int rawValue) noexcept
{
    for (const TimeScale timeScale : kKnownValues) {
        if (rawValue == static_cast<int>(timeScale)) {
            return timeScale;
        }
    }
    return std::nullopt;
}

int TimeScaleCodec::toRaw(const TimeScale timeScale) noexcept
{
    return isKnown(timeScale) ? static_cast<int>(timeScale) : kUnknownRawValue;
}

}  // namespace skygate::core
