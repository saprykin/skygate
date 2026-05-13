#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace skygate::ephemeris {

struct EphemerisTextDataAsset {
    std::string id;
    std::string version;
    std::string provenance;
    std::string content;
};

struct EphemerisKernelDataAsset {
    std::string id;
    std::string version;
    std::string provenance;
    std::string activePath;
};

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
