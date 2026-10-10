#include "StagedEphemerisAssetFixture.hpp"

#include "time/CalendarTime.hpp"

#include <QtGlobal>

#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr std::string_view kPayloadSha256 = "782092fd09110da30d95c1b5bc87cd827174ae5346bb3ea80cd5384fd8cf0d13";

}  // namespace

namespace skygate::ui::tests {

skygate::ephemeris::EphemerisDateRange StagedEphemerisAssetFixture::validityRange()
{
    const auto start = skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::core::CivilDateTime{
            .astronomicalYear = 2000,
            .month = 1,
            .day = 1,
            .timeScale = skygate::core::TimeScale::Utc,
        }
    );
    const auto end = skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::core::CivilDateTime{
            .astronomicalYear = 2100,
            .month = 1,
            .day = 1,
            .timeScale = skygate::core::TimeScale::Utc,
        }
    );
    Q_ASSERT(start.has_value());
    Q_ASSERT(end.has_value());
    return skygate::ephemeris::EphemerisDateRange{
        .id = "test-range",
        .displayName = "Test range",
        .start = *start,
        .end = *end,
    };
}

skygate::ephemeris::EphemerisDataManifest::Asset StagedEphemerisAssetFixture::stagedAsset(
    std::string id,
    const skygate::ephemeris::EphemerisDataManifest::AssetKind kind,
    std::string relativePath,
    std::string profileId
)
{
    skygate::ephemeris::EphemerisDataManifest::Asset asset;
    asset.id = std::move(id);
    asset.kind = kind;
    asset.profileId = std::move(profileId);
    asset.version = "test";
    asset.relativePath = std::move(relativePath);
    asset.checksum.algorithm = "sha256";
    asset.checksum.value = std::string{kPayloadSha256};
    asset.compression.kind = skygate::ephemeris::EphemerisDataManifest::CompressionKind::None;
    asset.compression.uncompressedSizeBytes = kPayload.size();
    asset.validityRange = validityRange();
    return asset;
}

}  // namespace skygate::ui::tests
