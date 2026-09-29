#pragma once

#include "IEarthOrientationProvider.hpp"

#include <vector>

namespace skygate::ephemeris {

struct EphemerisTextDataAsset;

class EarthOrientationDataParser final {
public:
    struct Result {
        IEarthOrientationProvider::DataInfo dataInfo;
        std::vector<IEarthOrientationProvider::TableEntry> entries;

        [[nodiscard]] bool isSuccess() const noexcept
        {
            return dataInfo.isUsable() && !entries.empty();
        }
    };

    [[nodiscard]] static Result parse(const EphemerisTextDataAsset& asset);
};

}  // namespace skygate::ephemeris
