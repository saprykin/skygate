#pragma once

#include "EphemerisKernelDataAsset.hpp"
#include "EphemerisTextDataAsset.hpp"

#include <optional>
#include <string_view>

namespace skygate::ephemeris {

class IEphemerisDataSnapshot {
public:
    virtual ~IEphemerisDataSnapshot() = default;

    [[nodiscard]] virtual std::optional<EphemerisTextDataAsset> leapSecondTableAsset() const = 0;
    [[nodiscard]] virtual std::optional<EphemerisTextDataAsset> deltaTDataAsset() const
    {
        return std::nullopt;
    }

    [[nodiscard]] virtual std::optional<EphemerisTextDataAsset> earthOrientationDataAsset() const
    {
        return std::nullopt;
    }

    [[nodiscard]] virtual std::optional<EphemerisKernelDataAsset> solarSystemKernelAsset(std::string_view) const
    {
        return std::nullopt;
    }
};

}  // namespace skygate::ephemeris
