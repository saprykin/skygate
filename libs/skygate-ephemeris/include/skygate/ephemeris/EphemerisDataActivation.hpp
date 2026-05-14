#pragma once

#include "skygate/ephemeris/EphemerisDataManifest.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
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

enum class EphemerisStagedUpdateVerificationStatus : std::uint8_t {
    Verified,
    InvalidRequest,
    UnsupportedProfile,
    IncompleteUpdateSet,
    WrongComponentKind,
    MissingAsset,
    MalformedMetadata,
    MismatchedMetadata,
    UnsupportedCompression,
    CorruptArchive,
    ChecksumMismatch,
    IoError
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

[[nodiscard]] constexpr std::string_view displayName(const EphemerisStagedUpdateVerificationStatus status) noexcept
{
    switch (status) {
    case EphemerisStagedUpdateVerificationStatus::Verified:
        return "verified";
    case EphemerisStagedUpdateVerificationStatus::InvalidRequest:
        return "invalid-request";
    case EphemerisStagedUpdateVerificationStatus::UnsupportedProfile:
        return "unsupported-profile";
    case EphemerisStagedUpdateVerificationStatus::IncompleteUpdateSet:
        return "incomplete-update-set";
    case EphemerisStagedUpdateVerificationStatus::WrongComponentKind:
        return "wrong-component-kind";
    case EphemerisStagedUpdateVerificationStatus::MissingAsset:
        return "missing-asset";
    case EphemerisStagedUpdateVerificationStatus::MalformedMetadata:
        return "malformed-metadata";
    case EphemerisStagedUpdateVerificationStatus::MismatchedMetadata:
        return "mismatched-metadata";
    case EphemerisStagedUpdateVerificationStatus::UnsupportedCompression:
        return "unsupported-compression";
    case EphemerisStagedUpdateVerificationStatus::CorruptArchive:
        return "corrupt-archive";
    case EphemerisStagedUpdateVerificationStatus::ChecksumMismatch:
        return "checksum-mismatch";
    case EphemerisStagedUpdateVerificationStatus::IoError:
        return "io-error";
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
};

struct EphemerisStagedUpdateVerificationResult {
    EphemerisStagedUpdateVerificationStatus status = EphemerisStagedUpdateVerificationStatus::InvalidRequest;
    std::vector<std::string> verifiedAssetIds;
    std::vector<std::string> diagnostics;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return status == EphemerisStagedUpdateVerificationStatus::Verified;
    }
};

[[nodiscard]] EphemerisDataActivationResult activateEphemerisDataAsset(const EphemerisDataActivationRequest& request);
[[nodiscard]] EphemerisStagedUpdateVerificationResult
verifyEphemerisStagedUpdateSet(const EphemerisStagedUpdateVerificationRequest& request);

}  // namespace skygate::ephemeris
