#pragma once

#include "skygate/ephemeris/EphemerisDataManifest.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

enum class EphemerisDataActivationStatus : std::uint8_t {
    Activated,
    AlreadyActive,
    InvalidRequest,
    MissingSource,
    UnsupportedCompression,
    CorruptArchive,
    ChecksumMismatch,
    IoError,
    LargeKernelInQtResource
};

[[nodiscard]] constexpr std::string_view displayName(const EphemerisDataActivationStatus status) noexcept
{
    switch (status) {
    case EphemerisDataActivationStatus::Activated:
        return "activated";
    case EphemerisDataActivationStatus::AlreadyActive:
        return "already-active";
    case EphemerisDataActivationStatus::InvalidRequest:
        return "invalid-request";
    case EphemerisDataActivationStatus::MissingSource:
        return "missing-source";
    case EphemerisDataActivationStatus::UnsupportedCompression:
        return "unsupported-compression";
    case EphemerisDataActivationStatus::CorruptArchive:
        return "corrupt-archive";
    case EphemerisDataActivationStatus::ChecksumMismatch:
        return "checksum-mismatch";
    case EphemerisDataActivationStatus::IoError:
        return "io-error";
    case EphemerisDataActivationStatus::LargeKernelInQtResource:
        return "large-kernel-in-qt-resource";
    }

    return {};
}

struct EphemerisDataActivationRequest {
    const EphemerisDataManifestAsset* asset = nullptr;
    std::filesystem::path bundledResourceRoot;
    std::filesystem::path writableCacheRoot;
    bool allowQtResourceKernelAssets = false;
    std::uint64_t largeKernelResourceThresholdBytes = 128ULL * 1024ULL * 1024ULL;
};

struct EphemerisDataActivationResult {
    EphemerisDataActivationStatus status = EphemerisDataActivationStatus::InvalidRequest;
    std::filesystem::path activePath;
    std::vector<std::string> diagnostics;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return status == EphemerisDataActivationStatus::Activated
               || status == EphemerisDataActivationStatus::AlreadyActive;
    }
};

[[nodiscard]] EphemerisDataActivationResult activateEphemerisDataAsset(const EphemerisDataActivationRequest& request);

}  // namespace skygate::ephemeris
