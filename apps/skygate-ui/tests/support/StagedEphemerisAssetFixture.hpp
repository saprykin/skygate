#pragma once

#include "engine/EphemerisDataManifest.hpp"
#include "time/EphemerisDateRange.hpp"

#include <string>
#include <string_view>

namespace skygate::ui::tests {

// Staged update payloads and asset fixtures shared by the ephemeris data
// manager suites.
class StagedEphemerisAssetFixture final {
public:
    static constexpr std::string_view kPayload = "SkyGate ephemeris staged asset\n";
    static constexpr std::string_view kEmptyPayloadSha256 =
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

    [[nodiscard]] static skygate::ephemeris::EphemerisDateRange validityRange();
    [[nodiscard]] static skygate::ephemeris::EphemerisDataManifest::Asset stagedAsset(
        std::string id,
        skygate::ephemeris::EphemerisDataManifest::AssetKind kind,
        std::string relativePath,
        std::string profileId = "de440s-short-range"
    );
};

}  // namespace skygate::ui::tests
