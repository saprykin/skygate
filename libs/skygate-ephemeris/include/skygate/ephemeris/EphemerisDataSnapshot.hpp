#pragma once

#include <optional>
#include <string>

namespace skygate::ephemeris {

struct EphemerisTextDataAsset {
    std::string id;
    std::string version;
    std::string provenance;
    std::string content;
};

class IEphemerisDataSnapshot {
public:
    virtual ~IEphemerisDataSnapshot() = default;

    [[nodiscard]] virtual std::optional<EphemerisTextDataAsset> leapSecondTableAsset() const = 0;
    [[nodiscard]] virtual std::optional<EphemerisTextDataAsset> deltaTDataAsset() const
    {
        return std::nullopt;
    }
};

}  // namespace skygate::ephemeris
