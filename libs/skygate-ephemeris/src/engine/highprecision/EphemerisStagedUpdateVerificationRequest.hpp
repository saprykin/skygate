#pragma once

#include "EphemerisDataManifest.hpp"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace skygate::ephemeris {

struct EphemerisStagedUpdateVerificationRequest {
    struct ExpectedComponent {
        std::string assetId;
        EphemerisDataManifestAssetKind kind = EphemerisDataManifestAssetKind::SolarSystemKernel;
        std::optional<std::string> expectedVersion;
        std::optional<EphemerisDateRange> requiredValidityRange;
    };

    const EphemerisDataManifest* manifest = nullptr;
    std::string profileId;
    std::filesystem::path stagedResourceRoot;
    std::vector<EphemerisDataManifestAssetKind> requiredKinds;
    std::vector<ExpectedComponent> expectedComponents;
    std::function<bool()> cancellationRequested;
};

}  // namespace skygate::ephemeris
