#pragma once

#include "EphemerisDataSnapshot.hpp"
#include "IDeltaTProvider.hpp"

#include <memory>
#include <optional>

namespace skygate::ephemeris {

class DeltaTDataLoader final {
public:
    struct Options {
        std::optional<AstronomicalEpoch> referenceEpoch;
    };

    struct Result {
        std::shared_ptr<const IDeltaTProvider> provider;
        IDeltaTProvider::DataInfo dataInfo;

        [[nodiscard]] bool isSuccess() const noexcept
        {
            return provider != nullptr && dataInfo.isUsable();
        }
    };

    [[nodiscard]] static Result loadFromSnapshot(const IEphemerisDataSnapshot& snapshot, const Options& options = {});

    [[nodiscard]] static Result loadFromTextAsset(const EphemerisTextDataAsset& asset, const Options& options = {});
};

}  // namespace skygate::ephemeris
