#pragma once

#include "EphemerisDataManifest.hpp"

namespace skygate::ephemeris {

/// Asset metadata rules that both the manifest reader and the staged-update
/// verifier apply to the assets they accept. Each caller keeps the wording of
/// its own diagnostics.
class EphemerisAssetMetadataPolicy final {
public:
    EphemerisAssetMetadataPolicy() = delete;

    /// An asset that is not live is served from a staged file, so it has to
    /// publish checksum metadata.
    [[nodiscard]] static bool requiresChecksum(const EphemerisDataManifest::Asset& asset) noexcept
    {
        return !asset.live;
    }

    /// A zstd asset sizes its staged file before and after decoding, so both
    /// sizes have to be published and positive.
    [[nodiscard]] static bool hasUsableCompressionSizes(const EphemerisDataManifest::Compression& compression) noexcept
    {
        return compression.kind != EphemerisDataManifest::CompressionKind::Zstd
               || (compression.compressedSizeBytes.has_value() && compression.uncompressedSizeBytes.has_value()
                   && *compression.compressedSizeBytes > 0U && *compression.uncompressedSizeBytes > 0U);
    }
};

}  // namespace skygate::ephemeris
