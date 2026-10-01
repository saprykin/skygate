#pragma once

#include "IEphemerisDataSnapshot.hpp"
#include "ILeapSecondProvider.hpp"

#include <memory>
#include <optional>

namespace skygate::ephemeris {

class LeapSecondTableLoader final {
public:
    struct Options {
        std::optional<AstronomicalEpoch> referenceEpoch;
    };

    struct Result {
        std::shared_ptr<const ILeapSecondProvider> provider;
        ILeapSecondProvider::TableInfo tableInfo;

        [[nodiscard]] bool isSuccess() const noexcept
        {
            return provider != nullptr && tableInfo.isUsable();
        }
    };

    [[nodiscard]] static Result loadFromSnapshot(const IEphemerisDataSnapshot& snapshot, const Options& options = {});

    [[nodiscard]] static Result loadFromTextAsset(const EphemerisTextDataAsset& asset, const Options& options = {});
};

}  // namespace skygate::ephemeris
