#pragma once

#include "SkyEphemerisDataCacheSnapshot.hpp"
#include "engine/EphemerisDataActivationStatus.hpp"
#include "engine/EphemerisDataManifest.hpp"
#include "engine/EphemerisStagedUpdateVerificationRequest.hpp"
#include "engine/EphemerisStagedUpdateVerificationStatus.hpp"

#include <QString>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class SkySettingsStore;

class SkyEphemerisCacheController final {
public:
    enum class StagedUpdateActivationStatus : std::uint8_t {
        Activated,
        InvalidRequest,
        VerificationFailed,
        ActivationFailed,
        PersistenceFailed,
        Canceled
    };

    struct StagedUpdateActivationRequest final {
        const skygate::ephemeris::EphemerisDataManifest* manifest = nullptr;
        QString profileId;
        QString stagedResourceRoot;
        QString writableCacheRoot;
        QString revisionToken;
        std::vector<skygate::ephemeris::EphemerisDataManifest::AssetKind> requiredKinds;
        std::vector<skygate::ephemeris::EphemerisStagedUpdateVerificationRequest::ExpectedComponent> expectedComponents;
        std::function<bool()> cancellationRequested;
        bool allowQtResourceKernelAssets = false;
        std::uint64_t largeKernelResourceThresholdBytes = 128ULL * 1024ULL * 1024ULL;
        bool retainStagedResourcesOnCancellation = true;
        bool cleanupFailedActivationCache = true;
    };

    struct StagedUpdateActivationResult final {
        StagedUpdateActivationStatus status = StagedUpdateActivationStatus::InvalidRequest;
        skygate::ephemeris::EphemerisStagedUpdateVerificationStatus verificationStatus =
            skygate::ephemeris::EphemerisStagedUpdateVerificationStatus::InvalidRequest;
        skygate::ephemeris::EphemerisDataActivationStatus activationStatus =
            skygate::ephemeris::EphemerisDataActivationStatus::InvalidRequest;
        SkyEphemerisDataCacheSnapshot cacheSnapshot;
        std::vector<QString> diagnostics;
        std::vector<QString> activatedAssetIds;

        [[nodiscard]] bool isSuccess() const noexcept
        {
            return status == StagedUpdateActivationStatus::Activated;
        }
    };

    [[nodiscard]] StagedUpdateActivationResult activate(
        const StagedUpdateActivationRequest& request,
        const SkyEphemerisDataCacheSnapshot& activeSnapshot,
        SkySettingsStore* settingsStore
    ) const;
};
