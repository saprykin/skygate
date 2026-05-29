#include "CalcephKernelProvider.hpp"
#include "math/PhysicalConstants.hpp"

#include <QByteArrayView>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
#include <calceph.h>
#endif

namespace skygate::ephemeris::highprecision {
namespace {

constexpr std::size_t kIoBufferBytes = 1U << 16U;

using skygate::core::PhysicalConstants;

void addDiagnostic(std::vector<std::string>& diagnostics, std::string diagnostic)
{
    diagnostics.push_back(std::move(diagnostic));
}

[[nodiscard]] QString pathToQString(const std::filesystem::path& path)
{
    return QString::fromStdString(path.generic_string());
}

[[nodiscard]] bool verifySha256File(const std::filesystem::path& path, const std::string& expectedHexDigest)
{
    QFile file(pathToQString(path));
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

[[nodiscard]] const EphemerisDataManifestProfile*
selectProfile(const EphemerisDataManifest& manifest, const CalcephKernelSelectionOptions& options) noexcept
{
    if (!options.preferredProfileId.empty()) {
        return manifest.profile(options.preferredProfileId);
    }

    if (options.preferLongRange) {
        const auto match = std::ranges::find_if(manifest.profiles, [](const EphemerisDataManifestProfile& profile) {
            return profile.longRange;
        });
        if (match != manifest.profiles.end()) {
            return &*match;
        }
    }

    const auto bundledShortRange =
        std::ranges::find_if(manifest.profiles, [](const EphemerisDataManifestProfile& profile) {
            return profile.bundled && !profile.longRange;
        });
    if (bundledShortRange != manifest.profiles.end()) {
        return &*bundledShortRange;
    }

    return manifest.profiles.empty() ? nullptr : &manifest.profiles.front();
}

[[nodiscard]] bool hasExplicitKernelSelection(const CalcephKernelSelectionOptions& options) noexcept
{
    return !options.preferredProfileId.empty() || options.preferLongRange;
}

[[nodiscard]] const EphemerisDataManifestAsset*
selectKernelAsset(const EphemerisDataManifest& manifest, const EphemerisDataManifestProfile& profile) noexcept
{
    for (const std::string& assetId : profile.assetIds) {
        const EphemerisDataManifestAsset* asset = manifest.asset(assetId);
        if (asset != nullptr && asset->kind == EphemerisDataManifestAssetKind::SolarSystemKernel) {
            return asset;
        }
    }

    return nullptr;
}

[[nodiscard]] std::optional<EphemerisKernelDataAsset> snapshotKernelForProfile(
    const IEphemerisDataSnapshot& snapshot,
    const EphemerisDataManifestAsset& asset,
    const EphemerisDataManifestProfile& profile
)
{
    std::optional<EphemerisKernelDataAsset> snapshotAsset = snapshot.solarSystemKernelAsset(asset.id);
    if (!snapshotAsset.has_value() || snapshotAsset->activePath.empty()) {
        return std::nullopt;
    }
    if (snapshotAsset->id != asset.id
        || (!snapshotAsset->profileId.empty() && snapshotAsset->profileId != profile.id)) {
        return std::nullopt;
    }

    return snapshotAsset;
}

#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
class CalcephRuntimeKernelHandle final : public ICalcephKernelHandle {
public:
    explicit CalcephRuntimeKernelHandle(t_calcephbin* handle) : m_handle(handle) {}

    ~CalcephRuntimeKernelHandle() override
    {
        if (m_handle != nullptr) {
            calceph_close(m_handle);
        }
    }

    CalcephRuntimeKernelHandle(const CalcephRuntimeKernelHandle&) = delete;
    CalcephRuntimeKernelHandle& operator=(const CalcephRuntimeKernelHandle&) = delete;

    [[nodiscard]] std::optional<skygate::core::Vector3d>
    computeGeometricState(const AstronomicalEpoch& epoch, const int targetNaifId, const int centerNaifId) const override
    {
        return computeGeometricStateWithVelocity(epoch, targetNaifId, centerNaifId).positionAu;
    }

    [[nodiscard]] SolarSystemKernelStateResult computeGeometricStateWithVelocity(
        const AstronomicalEpoch& epoch, const int targetNaifId, const int centerNaifId
    ) const override
    {
        SolarSystemKernelStateResult state;
        double positionVelocity[6]{};
        const int result = calceph_compute_unit(
            m_handle,
            epoch.julianDatePart1,
            epoch.julianDatePart2,
            targetNaifId,
            centerNaifId,
            CALCEPH_USE_NAIFID + CALCEPH_UNIT_KM + CALCEPH_UNIT_DAY,
            positionVelocity
        );
        if (result == 0) {
            return state;
        }

        state.positionAu = skygate::core::Vector3d{
            .x = positionVelocity[0] / PhysicalConstants::kAstronomicalUnitKilometers,
            .y = positionVelocity[1] / PhysicalConstants::kAstronomicalUnitKilometers,
            .z = positionVelocity[2] / PhysicalConstants::kAstronomicalUnitKilometers,
        };
        state.velocityAuPerDay = skygate::core::Vector3d{
            .x = positionVelocity[3] / PhysicalConstants::kAstronomicalUnitKilometers,
            .y = positionVelocity[4] / PhysicalConstants::kAstronomicalUnitKilometers,
            .z = positionVelocity[5] / PhysicalConstants::kAstronomicalUnitKilometers,
        };
        return state;
    }

private:
    t_calcephbin* m_handle = nullptr;
};
#endif

class CalcephRuntime final : public ICalcephKernelRuntime {
public:
    [[nodiscard]] bool isAvailable() const noexcept override
    {
#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
        return true;
#else
        return false;
#endif
    }

    [[nodiscard]] CalcephKernelOpenResult openKernel(const std::filesystem::path& path) const override
    {
#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
        t_calcephbin* handle = calceph_open(path.generic_string().c_str());
        if (handle == nullptr) {
            return {.diagnostic = "CALCEPH failed to open the selected solar-system kernel."};
        }

        return {.handle = std::make_unique<CalcephRuntimeKernelHandle>(handle)};
#else
        static_cast<void>(path);
        return {.diagnostic = "CALCEPH support is not enabled in this build."};
#endif
    }
};

}  // namespace

CalcephKernelProvider::CalcephKernelProvider(
    const IEphemerisDataSnapshot& snapshot,
    const EphemerisDataManifest& manifest,
    CalcephKernelSelectionOptions options,
    std::shared_ptr<const ICalcephKernelRuntime> runtime
)
{
    if (runtime == nullptr) {
        runtime = defaultCalcephKernelRuntime();
    }
    if (runtime == nullptr || !runtime->isAvailable()) {
        m_status = CalcephKernelProviderStatus::CalcephUnavailable;
        addDiagnostic(m_diagnostics, "CALCEPH support is not available for solar-system kernel loading.");
        return;
    }

    const EphemerisDataManifestProfile* profile = selectProfile(manifest, options);
    if (profile == nullptr) {
        m_status = CalcephKernelProviderStatus::MissingManifestProfile;
        addDiagnostic(m_diagnostics, "No ephemeris data profile is available for solar-system kernel selection.");
        return;
    }

    const EphemerisDataManifestAsset* manifestAsset = selectKernelAsset(manifest, *profile);
    if (manifestAsset == nullptr) {
        m_status = CalcephKernelProviderStatus::MissingKernelAsset;
        addDiagnostic(m_diagnostics, "Selected ephemeris data profile does not contain a solar-system kernel asset.");
        return;
    }
    if (manifestAsset->profileId != profile->id || manifestAsset->relativePath.empty()
        || manifestAsset->checksum.algorithm != "sha256" || manifestAsset->checksum.value.empty()) {
        m_status = CalcephKernelProviderStatus::InvalidKernelAsset;
        addDiagnostic(m_diagnostics, "Selected solar-system kernel asset metadata is incomplete or inconsistent.");
        return;
    }

    std::optional<EphemerisKernelDataAsset> snapshotAsset =
        snapshotKernelForProfile(snapshot, *manifestAsset, *profile);
    if (!snapshotAsset.has_value() && !hasExplicitKernelSelection(options)) {
        for (const EphemerisDataManifestProfile& candidateProfile : manifest.profiles) {
            const EphemerisDataManifestAsset* candidateAsset = selectKernelAsset(manifest, candidateProfile);
            if (candidateAsset == nullptr) {
                continue;
            }

            snapshotAsset = snapshotKernelForProfile(snapshot, *candidateAsset, candidateProfile);
            if (snapshotAsset.has_value()) {
                profile = &candidateProfile;
                manifestAsset = candidateAsset;
                break;
            }
        }
    }
    if (!snapshotAsset.has_value() || snapshotAsset->activePath.empty()) {
        m_status = CalcephKernelProviderStatus::MissingKernelFile;
        addDiagnostic(m_diagnostics, "The active ephemeris data snapshot does not expose the selected kernel file.");
        return;
    }
    if (snapshotAsset->id != manifestAsset->id
        || (!snapshotAsset->profileId.empty() && snapshotAsset->profileId != profile->id)) {
        m_status = CalcephKernelProviderStatus::InvalidKernelAsset;
        addDiagnostic(m_diagnostics, "The active ephemeris data snapshot exposed a different solar-system kernel.");
        return;
    }

    const std::filesystem::path activePath(snapshotAsset->activePath);
    const QFileInfo activeFileInfo(pathToQString(activePath));
    if (!activeFileInfo.exists() || !activeFileInfo.isFile()) {
        m_status = CalcephKernelProviderStatus::MissingKernelFile;
        addDiagnostic(m_diagnostics, "The selected solar-system kernel file does not exist.");
        return;
    }
    if (manifestAsset->compression.uncompressedSizeBytes.has_value()
        && activeFileInfo.size() != static_cast<qint64>(*manifestAsset->compression.uncompressedSizeBytes)) {
        m_status = CalcephKernelProviderStatus::InvalidKernelAsset;
        addDiagnostic(m_diagnostics, "The selected solar-system kernel file size does not match manifest metadata.");
        return;
    }
    if (options.verifyChecksum && !verifySha256File(activePath, manifestAsset->checksum.value)) {
        m_status = CalcephKernelProviderStatus::ChecksumMismatch;
        addDiagnostic(m_diagnostics, "The selected solar-system kernel checksum does not match manifest metadata.");
        return;
    }

    CalcephKernelOpenResult openResult = runtime->openKernel(activePath);
    if (!openResult.isSuccess()) {
        m_status = CalcephKernelProviderStatus::OpenFailed;
        addDiagnostic(
            m_diagnostics,
            openResult.diagnostic.empty() ? "Unable to open the selected solar-system kernel." : openResult.diagnostic
        );
        return;
    }

    m_kernelInfo = CalcephKernelInfo{
        .id = manifestAsset->id,
        .profileId = profile->id,
        .version = manifestAsset->version,
        .sourceUrl = manifestAsset->sourceUrl,
        .provenance = manifest.dataSetInfo.provenance,
        .activePath = activePath,
        .validityRange = manifestAsset->validityRange,
        .optional = manifestAsset->optional,
        .longRange = profile->longRange,
    };
    if (!snapshotAsset->version.empty()) {
        m_kernelInfo->version = snapshotAsset->version;
    }
    if (!snapshotAsset->provenance.empty()) {
        m_kernelInfo->provenance = snapshotAsset->provenance;
    }
    m_kernelHandle = std::move(openResult.handle);
    m_status = CalcephKernelProviderStatus::Ready;
}

CalcephKernelProvider::~CalcephKernelProvider() = default;
CalcephKernelProvider::CalcephKernelProvider(CalcephKernelProvider&&) noexcept = default;
CalcephKernelProvider& CalcephKernelProvider::operator=(CalcephKernelProvider&&) noexcept = default;

CalcephKernelProviderStatus CalcephKernelProvider::status() const noexcept
{
    return m_status;
}

bool CalcephKernelProvider::isReady() const noexcept
{
    return m_status == CalcephKernelProviderStatus::Ready && m_kernelHandle != nullptr && m_kernelInfo.has_value();
}

const std::vector<std::string>& CalcephKernelProvider::diagnostics() const noexcept
{
    return m_diagnostics;
}

const std::optional<CalcephKernelInfo>& CalcephKernelProvider::kernelInfo() const noexcept
{
    return m_kernelInfo;
}

CalcephKernelProviderStatus CalcephKernelProvider::statusForEpoch(const AstronomicalEpoch& epoch) const noexcept
{
    if (!isReady() || !m_kernelInfo.has_value()) {
        return m_status;
    }
    if (!epoch.isFinite() || epoch.sortKey() < m_kernelInfo->validityRange.start.sortKey()
        || epoch.sortKey() > m_kernelInfo->validityRange.end.sortKey()) {
        return CalcephKernelProviderStatus::OutOfRange;
    }

    return CalcephKernelProviderStatus::Ready;
}

SolarSystemKernelStateResult CalcephKernelProvider::computeGeometricState(
    const AstronomicalEpoch& epoch, const int targetNaifId, const int centerNaifId
) const
{
    SolarSystemKernelStateResult result;
    result.metadata.dataSourceProvenance = "CALCEPH solar-system kernel";

    if (epoch.timeScale != TimeScale::Tdb) {
        result.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
        result.metadata.addWarning(EphemerisEngineWarning::Code::TimeScaleDataUnavailable);
        return result;
    }

    const CalcephKernelProviderStatus epochStatus = statusForEpoch(epoch);
    if (epochStatus != CalcephKernelProviderStatus::Ready) {
        result.metadata.status = epochStatus == CalcephKernelProviderStatus::OutOfRange
                                     ? EphemerisEngineQueryStatus::Type::OutOfRange
                                     : EphemerisEngineQueryStatus::Type::Failed;
        result.metadata.addWarning(
            epochStatus == CalcephKernelProviderStatus::OutOfRange ? EphemerisEngineWarning::Code::DataOutOfRange
                                                                   : EphemerisEngineWarning::Code::MissingEphemerisData
        );
        return result;
    }

    if (m_kernelInfo.has_value()) {
        result.metadata.dataSourceProvenance = m_kernelInfo->provenance;
        result.metadata.effectiveDataValidityRange = m_kernelInfo->validityRange;
    }

    const SolarSystemKernelStateResult kernelState =
        m_kernelHandle->computeGeometricStateWithVelocity(epoch, targetNaifId, centerNaifId);
    result.positionAu = kernelState.positionAu;
    result.velocityAuPerDay = kernelState.velocityAuPerDay;
    if (!result.positionAu.has_value()) {
        result.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
        result.metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
        return result;
    }

    result.metadata.status = EphemerisEngineQueryStatus::Type::Valid;
    return result;
}

std::shared_ptr<const ICalcephKernelRuntime> defaultCalcephKernelRuntime()
{
    static const std::shared_ptr<const ICalcephKernelRuntime> kRuntime = std::make_shared<CalcephRuntime>();
    return kRuntime;
}

}  // namespace skygate::ephemeris::highprecision
