#pragma once

#include "engine/EphemerisDatasetInfo.hpp"
#include "time/EphemerisDateRange.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

class EphemerisDataManifest {
public:
    enum class Status : std::uint8_t {
        Valid,
        Malformed
    };

    enum class AssetKind : std::uint8_t {
        SolarSystemKernel,
        LeapSecondTable,
        EarthOrientationData,
        DeltaTData
    };

    enum class CompressionKind : std::uint8_t {
        None,
        Zstd
    };

    struct Checksum {
        std::string algorithm;
        std::string value;
    };

    struct Compression {
        CompressionKind kind = CompressionKind::None;
        std::optional<std::uint64_t> compressedSizeBytes;
        std::optional<std::uint64_t> uncompressedSizeBytes;
    };

    struct Asset {
        std::string id;
        AssetKind kind = AssetKind::SolarSystemKernel;
        std::string profileId;
        std::string version;
        std::string sourceUrl;
        std::string relativePath;
        Checksum checksum;
        Compression compression;
        EphemerisDateRange validityRange;
        bool optional = false;
        bool live = false;
    };

    struct Profile {
        std::string id;
        std::string displayName;
        bool bundled = false;
        bool longRange = false;
        std::vector<std::string> assetIds;
    };

    struct ParseResult;

    int schemaVersion = 1;
    EphemerisDatasetInfo dataSetInfo;
    std::vector<Profile> profiles;
    std::vector<Asset> assets;

    [[nodiscard]] const Profile* profile(std::string_view id) const noexcept;
    [[nodiscard]] const Asset* asset(std::string_view id) const noexcept;
    [[nodiscard]] static ParseResult parse(std::string_view payload);

private:
    class Parser;
};

struct EphemerisDataManifest::ParseResult {
    Status status = Status::Malformed;
    EphemerisDataManifest manifest;
    std::vector<std::string> diagnostics;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return status == Status::Valid && diagnostics.empty();
    }
};

}  // namespace skygate::ephemeris
