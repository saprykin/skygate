#include "EphemerisDataManifest.hpp"
#include "EphemerisManifestJsonReader.hpp"

#include <algorithm>
#include <optional>
#include <string_view>

namespace skygate::ephemeris {

std::optional<EphemerisDataManifest::AssetKind>
EphemerisDataManifest::assetKindFromText(const std::string_view text) noexcept
{
    if (text == "solar-system-kernel") {
        return AssetKind::SolarSystemKernel;
    }
    if (text == "leap-second-table") {
        return AssetKind::LeapSecondTable;
    }
    if (text == "earth-orientation-data") {
        return AssetKind::EarthOrientationData;
    }
    if (text == "delta-t-data") {
        return AssetKind::DeltaTData;
    }

    return std::nullopt;
}

std::string_view EphemerisDataManifest::assetKindToText(const AssetKind kind) noexcept
{
    switch (kind) {
    case AssetKind::SolarSystemKernel:
        return "solar-system-kernel";
    case AssetKind::LeapSecondTable:
        return "leap-second-table";
    case AssetKind::EarthOrientationData:
        return "earth-orientation-data";
    case AssetKind::DeltaTData:
        return "delta-t-data";
    }

    return {};
}

std::optional<EphemerisDataManifest::CompressionKind>
EphemerisDataManifest::compressionKindFromText(const std::string_view text) noexcept
{
    if (text == "none") {
        return CompressionKind::None;
    }
    if (text == "zstd") {
        return CompressionKind::Zstd;
    }

    return std::nullopt;
}

std::string_view EphemerisDataManifest::compressionKindToText(const CompressionKind kind) noexcept
{
    switch (kind) {
    case CompressionKind::None:
        return "none";
    case CompressionKind::Zstd:
        return "zstd";
    }

    return {};
}

const EphemerisDataManifest::Profile* EphemerisDataManifest::profile(const std::string_view id) const noexcept
{
    const auto match = std::ranges::find_if(profiles, [id](const EphemerisDataManifest::Profile& profile) {
        return profile.id == id;
    });
    return match == profiles.end() ? nullptr : &*match;
}

const EphemerisDataManifest::Asset* EphemerisDataManifest::asset(const std::string_view id) const noexcept
{
    const auto match =
        std::ranges::find_if(assets, [id](const EphemerisDataManifest::Asset& asset) { return asset.id == id; });
    return match == assets.end() ? nullptr : &*match;
}

EphemerisDataManifest::ParseResult EphemerisDataManifest::parse(const std::string_view payload)
{
    return EphemerisManifestJsonReader::read(payload);
}

}  // namespace skygate::ephemeris
