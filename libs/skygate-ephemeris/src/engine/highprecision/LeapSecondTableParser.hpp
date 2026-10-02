#pragma once

#include "ILeapSecondProvider.hpp"

#include <vector>

namespace skygate::ephemeris {

struct EphemerisTextDataAsset;

class LeapSecondTableParser final {
public:
    struct Result {
        ILeapSecondProvider::TableInfo tableInfo;
        std::vector<ILeapSecondProvider::TableEntry> entries;

        [[nodiscard]] bool isSuccess() const noexcept
        {
            return tableInfo.isUsable() && !entries.empty();
        }
    };

    [[nodiscard]] static Result parse(const EphemerisTextDataAsset& asset);
};

}  // namespace skygate::ephemeris
