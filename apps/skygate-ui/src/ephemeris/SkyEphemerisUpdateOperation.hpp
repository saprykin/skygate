#pragma once

#include "engine/EphemerisDataManifest.hpp"
#include "ephemeris/SkyEphemerisCacheController.hpp"

#include <QString>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace skygate::ui::internal {

// One ephemeris update run: the selected profile, the staged activation request
// assembled from downloaded assets, and the byte counters shared by the
// profile transfers.
//
// The operation is created for one manifest supplied by the workflow: either
// its refreshed copy or the controller's bundled fallback. The manifest
// outlives the operation; the operation holds only pointers into it.
class SkyEphemerisUpdateOperation final {
public:
    using Asset = skygate::ephemeris::EphemerisDataManifest::Asset;
    using Profile = skygate::ephemeris::EphemerisDataManifest::Profile;

    SkyEphemerisUpdateOperation(
        const skygate::ephemeris::EphemerisDataManifest& manifest,
        const Profile& profile,
        QString profileId,
        QString progressName,
        QString stagedRoot,
        QString writableCacheRoot
    )
        : m_manifest(&manifest), m_profile(&profile), m_progressName(std::move(progressName))
    {
        m_activationRequest.manifest = m_manifest;
        m_activationRequest.profileId = std::move(profileId);
        m_activationRequest.stagedResourceRoot = std::move(stagedRoot);
        m_activationRequest.writableCacheRoot = std::move(writableCacheRoot);
        m_activationRequest.revisionToken = QString::fromStdString(profile.id);

        for (const std::string& assetId : profile.assetIds) {
            const Asset* asset = manifest.asset(assetId);
            if (asset != nullptr && asset->compression.uncompressedSizeBytes.has_value()) {
                m_expectedBytes += *asset->compression.uncompressedSizeBytes;
            }
        }
    }

    [[nodiscard]] const QString& profileId() const noexcept
    {
        return m_activationRequest.profileId;
    }

    [[nodiscard]] const QString& progressName() const noexcept
    {
        return m_progressName;
    }

    [[nodiscard]] const QString& stagedRoot() const noexcept
    {
        return m_activationRequest.stagedResourceRoot;
    }

    [[nodiscard]] std::uint64_t expectedBytes() const noexcept
    {
        return m_expectedBytes;
    }

    [[nodiscard]] std::uint64_t completedBytes() const noexcept
    {
        return m_completedBytes;
    }

    [[nodiscard]] bool hasPendingAsset() const noexcept
    {
        return m_assetIndex < m_profile->assetIds.size();
    }

    [[nodiscard]] const Asset* currentAsset() const noexcept
    {
        if (!hasPendingAsset()) {
            return nullptr;
        }
        return m_manifest->asset(m_profile->assetIds[m_assetIndex]);
    }

    void advanceAsset() noexcept
    {
        ++m_assetIndex;
    }

    void recordStagedAsset(const Asset& asset, const std::uint64_t stagedBytes)
    {
        m_completedBytes += asset.compression.uncompressedSizeBytes.value_or(stagedBytes);
        m_activationRequest.requiredKinds.push_back(asset.kind);
        auto& component = m_activationRequest.expectedComponents.emplace_back(asset.id, asset.kind);
        component.expectedVersion = asset.version;
        component.requiredValidityRange = asset.validityRange;
    }

    [[nodiscard]] std::optional<double>
    stagedProgress(const std::uint64_t stagedBytes, const std::optional<std::uint64_t> totalBytes) const noexcept
    {
        const std::uint64_t denominator =
            m_expectedBytes > 0U ? m_expectedBytes : m_completedBytes + totalBytes.value_or(stagedBytes);
        if (denominator == 0U) {
            return std::nullopt;
        }
        return static_cast<double>(std::min(m_completedBytes + stagedBytes, denominator))
               / static_cast<double>(denominator);
    }

    [[nodiscard]] const SkyEphemerisCacheController::StagedUpdateActivationRequest& activationRequest() const noexcept
    {
        return m_activationRequest;
    }

private:
    const skygate::ephemeris::EphemerisDataManifest* m_manifest = nullptr;
    const Profile* m_profile = nullptr;
    SkyEphemerisCacheController::StagedUpdateActivationRequest m_activationRequest;
    QString m_progressName;
    std::size_t m_assetIndex = 0U;
    std::uint64_t m_expectedBytes = 0U;
    std::uint64_t m_completedBytes = 0U;
};

}  // namespace skygate::ui::internal
