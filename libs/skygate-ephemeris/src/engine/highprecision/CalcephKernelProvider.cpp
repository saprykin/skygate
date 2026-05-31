#include "CalcephKernelProvider.hpp"
#include "CalcephKernel.hpp"
#include "EphemerisDataManifest.hpp"
#include "EphemerisDataSnapshot.hpp"

#include <QByteArrayView>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace skygate::ephemeris::highprecision {

class CalcephKernelProvider::Impl final {
public:
    using Status = ICalcephKernel::Status;

    static constexpr std::size_t kIoBufferBytes = 1U << 16U;

    class FailedKernel final : public ICalcephKernel {
    public:
        FailedKernel(Status status, std::vector<std::string> diagnostics)
            : m_status(status == Status::Ready ? Status::OpenFailed : status), m_diagnostics(std::move(diagnostics))
        {
        }

        [[nodiscard]] Status status() const noexcept override
        {
            return m_status;
        }

        [[nodiscard]] const std::vector<std::string>& diagnostics() const noexcept override
        {
            return m_diagnostics;
        }

        [[nodiscard]] const std::optional<Info>& kernelInfo() const noexcept override
        {
            return m_kernelInfo;
        }

        [[nodiscard]] Status statusForEpoch(const AstronomicalEpoch& epoch) const noexcept override
        {
            Q_UNUSED(epoch)
            return m_status;
        }

        [[nodiscard]] SolarSystemKernelStateResult
        compute(const AstronomicalEpoch& epoch, int targetNaifId, int centerNaifId) const override
        {
            Q_UNUSED(epoch)
            Q_UNUSED(targetNaifId)
            Q_UNUSED(centerNaifId)

            SolarSystemKernelStateResult result;
            result.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
            result.metadata.addWarning(EphemerisEngineWarning::Code::MissingEphemerisData);
            return result;
        }

    private:
        Status m_status = Status::OpenFailed;
        std::vector<std::string> m_diagnostics;
        std::optional<Info> m_kernelInfo;
    };

    Impl(const IEphemerisDataSnapshot& snapshot, EphemerisDataManifest manifest, Options options)
        : m_manifest(std::move(manifest)), m_options(std::move(options)), m_kernelAssets(collectKernelAssets(snapshot))
    {
    }

    [[nodiscard]] std::shared_ptr<const ICalcephKernel> openKernel() const
    {
#if !defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
        return failedKernel(Status::CalcephUnavailable, "CALCEPH support is not enabled in this build.");
#else
        const EphemerisDataManifestProfile* profile = selectProfile();
        if (profile == nullptr) {
            return failedKernel(
                Status::MissingManifestProfile,
                "No ephemeris data profile is available for solar-system kernel selection."
            );
        }

        const EphemerisDataManifestAsset* manifestAsset = selectKernelAsset(*profile);
        if (manifestAsset == nullptr) {
            return failedKernel(
                Status::MissingKernelAsset,
                "Selected ephemeris data profile does not contain a solar-system kernel asset."
            );
        }
        if (manifestAsset->profileId != profile->id || manifestAsset->relativePath.empty()
            || manifestAsset->checksum.algorithm != "sha256" || manifestAsset->checksum.value.empty()) {
            return failedKernel(
                Status::InvalidKernelAsset, "Selected solar-system kernel asset metadata is incomplete or inconsistent."
            );
        }

        std::optional<EphemerisKernelDataAsset> snapshotAsset = snapshotKernelForProfile(*manifestAsset, *profile);
        if (!snapshotAsset.has_value() && !hasExplicitKernelSelection()) {
            for (const EphemerisDataManifestProfile& candidateProfile : m_manifest.profiles) {
                const EphemerisDataManifestAsset* candidateAsset = selectKernelAsset(candidateProfile);
                if (candidateAsset == nullptr) {
                    continue;
                }

                snapshotAsset = snapshotKernelForProfile(*candidateAsset, candidateProfile);
                if (snapshotAsset.has_value()) {
                    profile = &candidateProfile;
                    manifestAsset = candidateAsset;
                    break;
                }
            }
        }
        if (!snapshotAsset.has_value() || snapshotAsset->activePath.empty()) {
            return failedKernel(
                Status::MissingKernelFile,
                "The active ephemeris data snapshot does not expose the selected kernel file."
            );
        }
        if (snapshotAsset->id != manifestAsset->id
            || (!snapshotAsset->profileId.empty() && snapshotAsset->profileId != profile->id)) {
            return failedKernel(
                Status::InvalidKernelAsset,
                "The active ephemeris data snapshot exposed a different solar-system kernel."
            );
        }

        const std::filesystem::path activePath(snapshotAsset->activePath);
        const QFileInfo activeFileInfo(QString::fromStdString(activePath.generic_string()));
        if (!activeFileInfo.exists() || !activeFileInfo.isFile()) {
            return failedKernel(Status::MissingKernelFile, "The selected solar-system kernel file does not exist.");
        }
        if (manifestAsset->compression.uncompressedSizeBytes.has_value()
            && activeFileInfo.size() != static_cast<qint64>(*manifestAsset->compression.uncompressedSizeBytes)) {
            return failedKernel(
                Status::InvalidKernelAsset,
                "The selected solar-system kernel file size does not match manifest metadata."
            );
        }
        if (m_options.verifyChecksum && !verifySha256File(activePath, manifestAsset->checksum.value)) {
            return failedKernel(
                Status::ChecksumMismatch, "The selected solar-system kernel checksum does not match manifest metadata."
            );
        }

        ICalcephKernel::Info kernelInfo{
            .id = manifestAsset->id,
            .profileId = profile->id,
            .version = manifestAsset->version,
            .sourceUrl = manifestAsset->sourceUrl,
            .provenance = m_manifest.dataSetInfo.provenance,
            .activePath = activePath,
            .validityRange = manifestAsset->validityRange,
            .optional = manifestAsset->optional,
            .longRange = profile->longRange,
        };
        if (!snapshotAsset->version.empty()) {
            kernelInfo.version = snapshotAsset->version;
        }
        if (!snapshotAsset->provenance.empty()) {
            kernelInfo.provenance = snapshotAsset->provenance;
        }

        return std::make_shared<CalcephKernel>(std::move(kernelInfo));
#endif
    }

    [[nodiscard]] std::vector<EphemerisKernelDataAsset>
    collectKernelAssets(const IEphemerisDataSnapshot& snapshot) const
    {
        std::vector<EphemerisKernelDataAsset> assets;
        for (const EphemerisDataManifestAsset& manifestAsset : m_manifest.assets) {
            if (manifestAsset.kind != EphemerisDataManifestAssetKind::SolarSystemKernel) {
                continue;
            }

            std::optional<EphemerisKernelDataAsset> snapshotAsset = snapshot.solarSystemKernelAsset(manifestAsset.id);
            if (snapshotAsset.has_value()) {
                assets.push_back(std::move(*snapshotAsset));
            }
        }

        return assets;
    }

    [[nodiscard]] std::string_view displayName(const Status status) const noexcept
    {
        switch (status) {
        case Status::Ready:
            return "ready";
        case Status::CalcephUnavailable:
            return "calceph-unavailable";
        case Status::MissingManifestProfile:
            return "missing-manifest-profile";
        case Status::MissingKernelAsset:
            return "missing-kernel-asset";
        case Status::InvalidKernelAsset:
            return "invalid-kernel-asset";
        case Status::MissingKernelFile:
            return "missing-kernel-file";
        case Status::ChecksumMismatch:
            return "checksum-mismatch";
        case Status::OpenFailed:
            return "open-failed";
        case Status::OutOfRange:
            return "out-of-range";
        }

        return {};
    }

    [[nodiscard]] bool verifySha256File(const std::filesystem::path& path, const std::string& expectedHexDigest) const
    {
        QFile file(QString::fromStdString(path.generic_string()));
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }

        QCryptographicHash hash(QCryptographicHash::Sha256);
        std::array<char, kIoBufferBytes> buffer{};
        while (!file.atEnd()) {
            const qint64 bytesRead = file.read(buffer.data(), static_cast<qint64>(buffer.size()));
            if (bytesRead < 0) {
                return false;
            }
            hash.addData(QByteArrayView(buffer.data(), bytesRead));
        }

        return hash.result().toHex().toStdString() == expectedHexDigest;
    }

    [[nodiscard]] const EphemerisDataManifestProfile* selectProfile() const noexcept
    {
        if (!m_options.preferredProfileId.empty()) {
            return m_manifest.profile(m_options.preferredProfileId);
        }

        if (m_options.preferLongRange) {
            const auto match =
                std::ranges::find_if(m_manifest.profiles, [](const EphemerisDataManifestProfile& profile) {
                    return profile.longRange;
                });
            if (match != m_manifest.profiles.end()) {
                return &*match;
            }
        }

        const auto bundledShortRange =
            std::ranges::find_if(m_manifest.profiles, [](const EphemerisDataManifestProfile& profile) {
                return profile.bundled && !profile.longRange;
            });
        if (bundledShortRange != m_manifest.profiles.end()) {
            return &*bundledShortRange;
        }

        return m_manifest.profiles.empty() ? nullptr : &m_manifest.profiles.front();
    }

    [[nodiscard]] bool hasExplicitKernelSelection() const noexcept
    {
        return !m_options.preferredProfileId.empty() || m_options.preferLongRange;
    }

    [[nodiscard]] const EphemerisDataManifestAsset*
    selectKernelAsset(const EphemerisDataManifestProfile& profile) const noexcept
    {
        for (const std::string& assetId : profile.assetIds) {
            const EphemerisDataManifestAsset* asset = m_manifest.asset(assetId);
            if (asset != nullptr && asset->kind == EphemerisDataManifestAssetKind::SolarSystemKernel) {
                return asset;
            }
        }

        return nullptr;
    }

    [[nodiscard]] std::optional<EphemerisKernelDataAsset>
    snapshotKernelForProfile(const EphemerisDataManifestAsset& asset, const EphemerisDataManifestProfile& profile) const
    {
        const auto match =
            std::ranges::find_if(m_kernelAssets, [&asset](const EphemerisKernelDataAsset& snapshotAsset) {
                return snapshotAsset.id == asset.id;
            });
        if (match == m_kernelAssets.end() || match->activePath.empty()) {
            return std::nullopt;
        }
        if (!match->profileId.empty() && match->profileId != profile.id) {
            return std::nullopt;
        }

        return *match;
    }

    [[nodiscard]] std::shared_ptr<const ICalcephKernel> failedKernel(Status status, std::string diagnostic) const
    {
        std::vector<std::string> diagnostics;
        diagnostics.push_back(diagnostic.empty() ? std::string{displayName(status)} : std::move(diagnostic));
        return std::make_shared<FailedKernel>(status, std::move(diagnostics));
    }

    EphemerisDataManifest m_manifest;
    Options m_options;
    std::vector<EphemerisKernelDataAsset> m_kernelAssets;
};

CalcephKernelProvider::CalcephKernelProvider(
    const IEphemerisDataSnapshot& snapshot, const EphemerisDataManifest& manifest
)
    : CalcephKernelProvider(snapshot, manifest, Options{})
{
}

CalcephKernelProvider::CalcephKernelProvider(
    const IEphemerisDataSnapshot& snapshot, const EphemerisDataManifest& manifest, Options options
)
    : m_impl(std::make_unique<Impl>(snapshot, manifest, std::move(options)))
{
}

CalcephKernelProvider::~CalcephKernelProvider() = default;

std::shared_ptr<const ICalcephKernel> CalcephKernelProvider::openKernel() const
{
    return m_impl->openKernel();
}

}  // namespace skygate::ephemeris::highprecision
