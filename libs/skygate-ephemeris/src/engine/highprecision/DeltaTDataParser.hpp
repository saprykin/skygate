#pragma once

#include "IDeltaTProvider.hpp"

#include <vector>

namespace skygate::ephemeris {

struct EphemerisTextDataAsset;

class DeltaTDataParser final {
public:
    struct Result {
        IDeltaTProvider::DataInfo dataInfo;
        std::vector<IDeltaTProvider::TableEntry> entries;

        [[nodiscard]] bool isSuccess() const noexcept
        {
            return dataInfo.isUsable() && !entries.empty();
        }
    };

    [[nodiscard]] static Result parse(const EphemerisTextDataAsset& asset);
};

}  // namespace skygate::ephemeris
