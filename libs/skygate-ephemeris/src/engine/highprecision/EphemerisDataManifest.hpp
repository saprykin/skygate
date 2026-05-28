#pragma once

#include "engine/EphemerisDatasetInfo.hpp"
#include "engine/EphemerisDateRange.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

enum class EphemerisDataManifestStatus : std::uint8_t {
    Valid,
    Malformed
};

[[nodiscard]] constexpr std::string_view displayName(const EphemerisDataManifestStatus status) noexcept
{
    switch (status) {
    case EphemerisDataManifestStatus::Valid:
        return "valid";
    case EphemerisDataManifestStatus::Malformed:
        return "malformed";
    }

    return {};
}

enum class EphemerisDataManifestAssetKind : std::uint8_t {
    SolarSystemKernel,
    LeapSecondTable,
    EarthOrientationData,
    DeltaTData
};

[[nodiscard]] constexpr std::string_view displayName(const EphemerisDataManifestAssetKind kind) noexcept
{
    switch (kind) {
    case EphemerisDataManifestAssetKind::SolarSystemKernel:
        return "solar-system-kernel";
    case EphemerisDataManifestAssetKind::LeapSecondTable:
        return "leap-second-table";
    case EphemerisDataManifestAssetKind::EarthOrientationData:
        return "earth-orientation-data";
    case EphemerisDataManifestAssetKind::DeltaTData:
        return "delta-t-data";
    }

    return {};
}

enum class EphemerisDataManifestCompressionKind : std::uint8_t {
    None,
    Zstd
};

[[nodiscard]] constexpr std::string_view displayName(const EphemerisDataManifestCompressionKind kind) noexcept
{
    switch (kind) {
    case EphemerisDataManifestCompressionKind::None:
        return "none";
    case EphemerisDataManifestCompressionKind::Zstd:
        return "zstd";
    }

    return {};
}

struct EphemerisDataManifestChecksum {
    std::string algorithm;
    std::string value;
};

struct EphemerisDataManifestCompression {
    EphemerisDataManifestCompressionKind kind = EphemerisDataManifestCompressionKind::None;
    std::optional<std::uint64_t> compressedSizeBytes;
    std::optional<std::uint64_t> uncompressedSizeBytes;
};

struct EphemerisDataManifestAsset {
    std::string id;
    EphemerisDataManifestAssetKind kind = EphemerisDataManifestAssetKind::SolarSystemKernel;
    std::string profileId;
    std::string version;
    std::string sourceUrl;
    std::string relativePath;
    EphemerisDataManifestChecksum checksum;
    EphemerisDataManifestCompression compression;
    EphemerisDateRange validityRange;
    bool optional = false;
};

struct EphemerisDataManifestProfile {
    std::string id;
    std::string displayName;
    bool bundled = false;
    bool longRange = false;
    std::vector<std::string> assetIds;
};

struct EphemerisDataManifest {
    int schemaVersion = 1;
    EphemerisDatasetInfo dataSetInfo;
    std::vector<EphemerisDataManifestProfile> profiles;
    std::vector<EphemerisDataManifestAsset> assets;

    [[nodiscard]] const EphemerisDataManifestProfile* profile(std::string_view id) const noexcept;
    [[nodiscard]] const EphemerisDataManifestAsset* asset(std::string_view id) const noexcept;
};

struct EphemerisDataManifestParseResult {
    EphemerisDataManifestStatus status = EphemerisDataManifestStatus::Malformed;
    EphemerisDataManifest manifest;
    std::vector<std::string> diagnostics;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return status == EphemerisDataManifestStatus::Valid && diagnostics.empty();
    }
};

[[nodiscard]] EphemerisDataManifestParseResult parseEphemerisDataManifest(std::string_view payload);

}  // namespace skygate::ephemeris
