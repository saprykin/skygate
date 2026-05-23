#include "engine/highprecision/EphemerisDataActivation.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace {

constexpr std::string_view kPayload = "SkyGate ephemeris bundled asset\nline two\n";
constexpr std::string_view kPayloadSha256 = "46886c2ebd5fefef66b3283a6f45f10cd4e1520fa1bc47816bfa3bb6b39eed98";
constexpr std::array<unsigned char, 50> kCompressedPayload{
    0x28, 0xb5, 0x2f, 0xfd, 0x20, 0x29, 0x49, 0x01, 0x00, 0x53, 0x6b, 0x79, 0x47, 0x61, 0x74, 0x65, 0x20,
    0x65, 0x70, 0x68, 0x65, 0x6d, 0x65, 0x72, 0x69, 0x73, 0x20, 0x62, 0x75, 0x6e, 0x64, 0x6c, 0x65, 0x64,
    0x20, 0x61, 0x73, 0x73, 0x65, 0x74, 0x0a, 0x6c, 0x69, 0x6e, 0x65, 0x20, 0x74, 0x77, 0x6f, 0x0a,
};

[[nodiscard]] std::filesystem::path pathFromQString(const QString& path)
{
    return std::filesystem::path(path.toStdString());
}

[[nodiscard]] QString pathToQString(const std::filesystem::path& path)
{
    return QString::fromStdString(path.generic_string());
}

void writeFile(const QString& path, const QByteArray& payload)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(payload), static_cast<qint64>(payload.size()));
}

[[nodiscard]] QByteArray readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

[[nodiscard]] bool containsFiles(const QString& rootPath)
{
    QDirIterator iterator(rootPath, QDir::Files, QDirIterator::Subdirectories);
    return iterator.hasNext();
}

[[nodiscard]] skygate::ephemeris::EphemerisDataManifestAsset makeZstdAsset()
{
    skygate::ephemeris::EphemerisDataManifestAsset asset;
    asset.id = "de440s-kernel";
    asset.kind = skygate::ephemeris::EphemerisDataManifestAssetKind::SolarSystemKernel;
    asset.profileId = "modern";
    asset.version = "test";
    asset.relativePath = "kernels/de440s.bsp.zst";
    asset.checksum.algorithm = "sha256";
    asset.checksum.value = std::string{kPayloadSha256};
    asset.compression.kind = skygate::ephemeris::EphemerisDataManifestCompressionKind::Zstd;
    asset.compression.compressedSizeBytes = kCompressedPayload.size();
    asset.compression.uncompressedSizeBytes = kPayload.size();
    return asset;
}

[[nodiscard]] skygate::ephemeris::EphemerisDateRange makeValidityRange(
    const int startYear, const int endYear, std::string id = "test-range", std::string displayName = "Test range"
)
{
    const auto start = skygate::ephemeris::astronomicalEpochFromCivilDateTime(skygate::ephemeris::CivilDateTime{
        .astronomicalYear = startYear,
        .month = 1,
        .day = 1,
        .timeScale = skygate::ephemeris::TimeScale::Utc,
    });
    const auto end = skygate::ephemeris::astronomicalEpochFromCivilDateTime(skygate::ephemeris::CivilDateTime{
        .astronomicalYear = endYear,
        .month = 1,
        .day = 1,
        .timeScale = skygate::ephemeris::TimeScale::Utc,
    });
    Q_ASSERT(start.has_value());
    Q_ASSERT(end.has_value());
    return skygate::ephemeris::EphemerisDateRange{
        .id = std::move(id),
        .displayName = std::move(displayName),
        .start = *start,
        .end = *end,
    };
}

[[nodiscard]] skygate::ephemeris::EphemerisDateRange testValidityRange()
{
    return makeValidityRange(2000, 2100);
}

[[nodiscard]] skygate::ephemeris::EphemerisDataManifestAsset makeUncompressedAsset()
{
    skygate::ephemeris::EphemerisDataManifestAsset asset = makeZstdAsset();
    asset.relativePath = "kernels/de440s.bsp";
    asset.compression.kind = skygate::ephemeris::EphemerisDataManifestCompressionKind::None;
    asset.compression.compressedSizeBytes.reset();
    asset.compression.uncompressedSizeBytes = kPayload.size();
    asset.validityRange = testValidityRange();
    return asset;
}

[[nodiscard]] skygate::ephemeris::EphemerisDataManifestAsset makeUncompressedAsset(
    std::string id, const skygate::ephemeris::EphemerisDataManifestAssetKind kind, std::string relativePath
)
{
    skygate::ephemeris::EphemerisDataManifestAsset asset = makeUncompressedAsset();
    asset.id = std::move(id);
    asset.kind = kind;
    asset.relativePath = std::move(relativePath);
    return asset;
}

[[nodiscard]] skygate::ephemeris::EphemerisDataManifest makeStagedManifest()
{
    skygate::ephemeris::EphemerisDataManifest manifest;
    manifest.dataSetInfo.id = "test-data";
    manifest.dataSetInfo.displayName = "Test data";
    manifest.dataSetInfo.version = "2026a";
    manifest.dataSetInfo.provenance = "test";
    manifest.profiles.push_back(skygate::ephemeris::EphemerisDataManifestProfile{
        .id = "modern",
        .displayName = "Modern",
        .bundled = false,
        .longRange = false,
        .assetIds = {"de440s-kernel", "leap-seconds", "earth-orientation", "delta-t"},
    });
    manifest.assets.push_back(makeUncompressedAsset(
        "de440s-kernel", skygate::ephemeris::EphemerisDataManifestAssetKind::SolarSystemKernel, "kernels/de440s.bsp"
    ));
    manifest.assets.push_back(makeUncompressedAsset(
        "leap-seconds", skygate::ephemeris::EphemerisDataManifestAssetKind::LeapSecondTable, "time/leap-seconds.list"
    ));
    manifest.assets.push_back(makeUncompressedAsset(
        "earth-orientation", skygate::ephemeris::EphemerisDataManifestAssetKind::EarthOrientationData, "time/eop.csv"
    ));
    manifest.assets.push_back(makeUncompressedAsset(
        "delta-t", skygate::ephemeris::EphemerisDataManifestAssetKind::DeltaTData, "time/delta-t.csv"
    ));
    return manifest;
}

[[nodiscard]] skygate::ephemeris::EphemerisDataActivationRequest makeRequest(
    const skygate::ephemeris::EphemerisDataManifestAsset& asset,
    const QTemporaryDir& bundledRoot,
    const QTemporaryDir& cacheRoot
)
{
    return {
        .asset = &asset,
        .bundledResourceRoot = pathFromQString(bundledRoot.path()),
        .writableCacheRoot = pathFromQString(cacheRoot.path()),
    };
}

[[nodiscard]] QByteArray compressedPayload()
{
    return QByteArray(
        reinterpret_cast<const char*>(kCompressedPayload.data()), static_cast<qsizetype>(kCompressedPayload.size())
    );
}

void writeStagedAsset(const QTemporaryDir& root, const skygate::ephemeris::EphemerisDataManifestAsset& asset)
{
    const QString path = root.path() + QStringLiteral("/") + QString::fromStdString(asset.relativePath);
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    writeFile(path, QByteArray(kPayload.data(), static_cast<qsizetype>(kPayload.size())));
}

void writeAllStagedAssets(const QTemporaryDir& root, const skygate::ephemeris::EphemerisDataManifest& manifest)
{
    for (const skygate::ephemeris::EphemerisDataManifestAsset& asset : manifest.assets) {
        writeStagedAsset(root, asset);
    }
}

[[nodiscard]] skygate::ephemeris::EphemerisStagedUpdateVerificationRequest
verificationRequest(const skygate::ephemeris::EphemerisDataManifest& manifest, const QTemporaryDir& stagedRoot)
{
    skygate::ephemeris::EphemerisStagedUpdateVerificationRequest request{
        .manifest = &manifest,
        .profileId = "modern",
        .stagedResourceRoot = pathFromQString(stagedRoot.path()),
        .requiredKinds =
            {
                skygate::ephemeris::EphemerisDataManifestAssetKind::SolarSystemKernel,
                skygate::ephemeris::EphemerisDataManifestAssetKind::LeapSecondTable,
                skygate::ephemeris::EphemerisDataManifestAssetKind::EarthOrientationData,
                skygate::ephemeris::EphemerisDataManifestAssetKind::DeltaTData,
            },
        .expectedComponents =
            {
                {"de440s-kernel", skygate::ephemeris::EphemerisDataManifestAssetKind::SolarSystemKernel},
                {"leap-seconds", skygate::ephemeris::EphemerisDataManifestAssetKind::LeapSecondTable},
                {"earth-orientation", skygate::ephemeris::EphemerisDataManifestAssetKind::EarthOrientationData},
                {"delta-t", skygate::ephemeris::EphemerisDataManifestAssetKind::DeltaTData},
            },
    };

    for (skygate::ephemeris::EphemerisStagedUpdateVerificationRequest::ExpectedComponent& component :
         request.expectedComponents) {
        component.expectedVersion = "test";
        component.requiredValidityRange = testValidityRange();
    }

    return request;
}

[[nodiscard]] QString sourcePath(const QTemporaryDir& root)
{
    return root.path() + QStringLiteral("/kernels/de440s.bsp.zst");
}

[[nodiscard]] QString uncompressedSourcePath(const QTemporaryDir& root)
{
    return root.path() + QStringLiteral("/kernels/de440s.bsp");
}

}  // namespace

class EphemerisDataActivationTests final : public QObject {
    Q_OBJECT

private slots:
    void activatesValidZstdArchive();
    void rejectsCorruptZstdArchive();
    void rejectsChecksumMismatch();
    void rejectsExpectedSizeMismatch();
    void preservesExistingCacheFileWhenReplacementCannotBeWritten();
    void treatsExistingValidCacheFileAsAlreadyActive();
    void cancelsActivationBeforeWritingCacheFile();
    void cancelsActivationAfterPayloadCopyBeforeCommit();
    void rejectsLargeKernelQtResourcePaths();
    void verifiesCompleteStagedUpdateSet();
    void cancelsStagedUpdateVerification();
    void rejectsStagedUpdateChecksumFailure();
    void rejectsStagedUpdateWrongComponentKind();
    void rejectsStagedUpdateVersionMismatch();
    void rejectsStagedUpdateValidityRangeMismatch();
    void rejectsIncompleteStagedUpdateSet();
    void rejectsMalformedStagedMetadata();
    void rejectsMalformedStagedValidityRangeLabels();
    void rejectsCorruptCompressedStagedAsset();
    void rejectsUnsupportedStagedProfile();
};

void EphemerisDataActivationTests::activatesValidZstdArchive()
{
    QTemporaryDir bundledRoot;
    QTemporaryDir cacheRoot;
    QVERIFY(bundledRoot.isValid());
    QVERIFY(cacheRoot.isValid());
    QVERIFY(QDir(bundledRoot.path()).mkpath(QStringLiteral("kernels")));
    const skygate::ephemeris::EphemerisDataManifestAsset asset = makeZstdAsset();
    writeFile(sourcePath(bundledRoot), compressedPayload());

    const skygate::ephemeris::EphemerisDataActivationResult result =
        skygate::ephemeris::activateEphemerisDataAsset(makeRequest(asset, bundledRoot, cacheRoot));

    if (result.status == skygate::ephemeris::EphemerisDataActivationStatus::UnsupportedCompression) {
        QSKIP("zstd runtime library is unavailable in this test environment.");
    }
    QVERIFY2(result.isSuccess(), result.diagnostics.empty() ? "" : result.diagnostics.front().c_str());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisDataActivationStatus::Activated)
    );
    QFile activeFile(QString::fromStdString(result.activePath.generic_string()));
    QVERIFY(activeFile.open(QIODevice::ReadOnly));
    QCOMPARE(activeFile.readAll(), QByteArray(kPayload.data(), static_cast<qsizetype>(kPayload.size())));
    QVERIFY(result.activePath.generic_string().ends_with("modern/kernels/de440s.bsp"));
}

void EphemerisDataActivationTests::rejectsCorruptZstdArchive()
{
    QTemporaryDir bundledRoot;
    QTemporaryDir cacheRoot;
    QVERIFY(bundledRoot.isValid());
    QVERIFY(cacheRoot.isValid());
    QVERIFY(QDir(bundledRoot.path()).mkpath(QStringLiteral("kernels")));
    skygate::ephemeris::EphemerisDataManifestAsset asset = makeZstdAsset();
    QByteArray corruptPayload = compressedPayload();
    corruptPayload.fill('x');
    writeFile(sourcePath(bundledRoot), corruptPayload);

    const skygate::ephemeris::EphemerisDataActivationResult result =
        skygate::ephemeris::activateEphemerisDataAsset(makeRequest(asset, bundledRoot, cacheRoot));

    if (result.status == skygate::ephemeris::EphemerisDataActivationStatus::UnsupportedCompression) {
        QSKIP("zstd runtime library is unavailable in this test environment.");
    }
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisDataActivationStatus::CorruptArchive)
    );
    QVERIFY(!result.diagnostics.empty());
    QVERIFY(!QFileInfo::exists(pathToQString(result.activePath)));
    QVERIFY(!containsFiles(cacheRoot.path()));
}

void EphemerisDataActivationTests::rejectsChecksumMismatch()
{
    QTemporaryDir bundledRoot;
    QTemporaryDir cacheRoot;
    QVERIFY(bundledRoot.isValid());
    QVERIFY(cacheRoot.isValid());
    QVERIFY(QDir(bundledRoot.path()).mkpath(QStringLiteral("kernels")));
    skygate::ephemeris::EphemerisDataManifestAsset asset = makeZstdAsset();
    asset.checksum.value = std::string(64U, '0');
    writeFile(sourcePath(bundledRoot), compressedPayload());

    const skygate::ephemeris::EphemerisDataActivationResult result =
        skygate::ephemeris::activateEphemerisDataAsset(makeRequest(asset, bundledRoot, cacheRoot));

    if (result.status == skygate::ephemeris::EphemerisDataActivationStatus::UnsupportedCompression) {
        QSKIP("zstd runtime library is unavailable in this test environment.");
    }
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisDataActivationStatus::ChecksumMismatch)
    );
    QVERIFY(!result.diagnostics.empty());
    QVERIFY(!QFileInfo::exists(pathToQString(result.activePath)));
    QVERIFY(!containsFiles(cacheRoot.path()));
}

void EphemerisDataActivationTests::rejectsExpectedSizeMismatch()
{
    QTemporaryDir bundledRoot;
    QTemporaryDir cacheRoot;
    QVERIFY(bundledRoot.isValid());
    QVERIFY(cacheRoot.isValid());
    QVERIFY(QDir(bundledRoot.path()).mkpath(QStringLiteral("kernels")));
    skygate::ephemeris::EphemerisDataManifestAsset asset = makeUncompressedAsset();
    asset.compression.uncompressedSizeBytes = kPayload.size() + 1U;
    writeFile(
        uncompressedSourcePath(bundledRoot), QByteArray(kPayload.data(), static_cast<qsizetype>(kPayload.size()))
    );

    const skygate::ephemeris::EphemerisDataActivationResult result =
        skygate::ephemeris::activateEphemerisDataAsset(makeRequest(asset, bundledRoot, cacheRoot));

    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisDataActivationStatus::ChecksumMismatch)
    );
    QVERIFY(!result.diagnostics.empty());
    QVERIFY(!QFileInfo::exists(pathToQString(result.activePath)));
    QVERIFY(!containsFiles(cacheRoot.path()));
}

void EphemerisDataActivationTests::preservesExistingCacheFileWhenReplacementCannotBeWritten()
{
    QTemporaryDir bundledRoot;
    QTemporaryDir cacheRoot;
    QVERIFY(bundledRoot.isValid());
    QVERIFY(cacheRoot.isValid());
    QVERIFY(QDir(bundledRoot.path()).mkpath(QStringLiteral("kernels")));
    const skygate::ephemeris::EphemerisDataManifestAsset asset = makeUncompressedAsset();
    writeFile(
        uncompressedSourcePath(bundledRoot), QByteArray(kPayload.data(), static_cast<qsizetype>(kPayload.size()))
    );

    const QString activePath = cacheRoot.path() + QStringLiteral("/modern/kernels/de440s.bsp");
    QVERIFY(QDir(cacheRoot.path()).mkpath(QStringLiteral("modern/kernels")));
    const QByteArray previousActivePayload("previous active cache");
    writeFile(activePath, previousActivePayload);

    const QString activeDirectoryPath = QFileInfo(activePath).absolutePath();
    const QFileDevice::Permissions originalPermissions = QFileInfo(activeDirectoryPath).permissions();
    const QFileDevice::Permissions readOnlyPermissions =
        originalPermissions
        & ~(QFileDevice::WriteOwner | QFileDevice::WriteUser | QFileDevice::WriteGroup | QFileDevice::WriteOther);
    QVERIFY(QFile::setPermissions(activeDirectoryPath, readOnlyPermissions));

    const skygate::ephemeris::EphemerisDataActivationResult result =
        skygate::ephemeris::activateEphemerisDataAsset(makeRequest(asset, bundledRoot, cacheRoot));

    QVERIFY(QFile::setPermissions(activeDirectoryPath, originalPermissions));
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisDataActivationStatus::IoError)
    );
    QVERIFY(!result.diagnostics.empty());
    QCOMPARE(readFile(activePath), previousActivePayload);
}

void EphemerisDataActivationTests::treatsExistingValidCacheFileAsAlreadyActive()
{
    QTemporaryDir bundledRoot;
    QTemporaryDir cacheRoot;
    QVERIFY(bundledRoot.isValid());
    QVERIFY(cacheRoot.isValid());
    QVERIFY(QDir(bundledRoot.path()).mkpath(QStringLiteral("kernels")));
    const skygate::ephemeris::EphemerisDataManifestAsset asset = makeZstdAsset();
    writeFile(sourcePath(bundledRoot), compressedPayload());

    const skygate::ephemeris::EphemerisDataActivationRequest request = makeRequest(asset, bundledRoot, cacheRoot);
    const skygate::ephemeris::EphemerisDataActivationResult firstResult =
        skygate::ephemeris::activateEphemerisDataAsset(request);
    if (firstResult.status == skygate::ephemeris::EphemerisDataActivationStatus::UnsupportedCompression) {
        QSKIP("zstd runtime library is unavailable in this test environment.");
    }
    QCOMPARE(
        static_cast<std::uint8_t>(firstResult.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisDataActivationStatus::Activated)
    );

    const skygate::ephemeris::EphemerisDataActivationResult secondResult =
        skygate::ephemeris::activateEphemerisDataAsset(request);

    QCOMPARE(
        static_cast<std::uint8_t>(secondResult.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisDataActivationStatus::AlreadyActive)
    );
    QVERIFY(secondResult.activePath == firstResult.activePath);
}

void EphemerisDataActivationTests::cancelsActivationBeforeWritingCacheFile()
{
    QTemporaryDir bundledRoot;
    QTemporaryDir cacheRoot;
    QVERIFY(bundledRoot.isValid());
    QVERIFY(cacheRoot.isValid());
    QVERIFY(QDir(bundledRoot.path()).mkpath(QStringLiteral("kernels")));
    const skygate::ephemeris::EphemerisDataManifestAsset asset = makeUncompressedAsset();
    writeFile(
        uncompressedSourcePath(bundledRoot), QByteArray(kPayload.data(), static_cast<qsizetype>(kPayload.size()))
    );

    skygate::ephemeris::EphemerisDataActivationRequest request = makeRequest(asset, bundledRoot, cacheRoot);
    request.cancellationRequested = [] { return true; };

    const skygate::ephemeris::EphemerisDataActivationResult result =
        skygate::ephemeris::activateEphemerisDataAsset(request);

    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisDataActivationStatus::Canceled)
    );
    QVERIFY(!result.diagnostics.empty());
    QVERIFY(!containsFiles(cacheRoot.path()));
}

void EphemerisDataActivationTests::cancelsActivationAfterPayloadCopyBeforeCommit()
{
    QTemporaryDir bundledRoot;
    QTemporaryDir cacheRoot;
    QVERIFY(bundledRoot.isValid());
    QVERIFY(cacheRoot.isValid());
    QVERIFY(QDir(bundledRoot.path()).mkpath(QStringLiteral("kernels")));
    const skygate::ephemeris::EphemerisDataManifestAsset asset = makeUncompressedAsset();
    writeFile(
        uncompressedSourcePath(bundledRoot), QByteArray(kPayload.data(), static_cast<qsizetype>(kPayload.size()))
    );

    int cancellationChecks = 0;
    skygate::ephemeris::EphemerisDataActivationRequest request = makeRequest(asset, bundledRoot, cacheRoot);
    request.cancellationRequested = [&cancellationChecks] {
        ++cancellationChecks;
        return cancellationChecks >= 3;
    };

    const skygate::ephemeris::EphemerisDataActivationResult result =
        skygate::ephemeris::activateEphemerisDataAsset(request);

    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisDataActivationStatus::Canceled)
    );
    QVERIFY(!result.diagnostics.empty());
    QVERIFY(!QFileInfo::exists(pathToQString(result.activePath)));
    QVERIFY(!containsFiles(cacheRoot.path()));
}

void EphemerisDataActivationTests::rejectsLargeKernelQtResourcePaths()
{
    QTemporaryDir cacheRoot;
    QVERIFY(cacheRoot.isValid());
    skygate::ephemeris::EphemerisDataManifestAsset asset = makeZstdAsset();
    asset.compression.uncompressedSizeBytes = 2ULL * 1024ULL * 1024ULL * 1024ULL;
    const skygate::ephemeris::EphemerisDataActivationRequest request{
        .asset = &asset,
        .bundledResourceRoot = std::filesystem::path(":/ephemeris"),
        .writableCacheRoot = pathFromQString(cacheRoot.path()),
    };

    const skygate::ephemeris::EphemerisDataActivationResult result =
        skygate::ephemeris::activateEphemerisDataAsset(request);

    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisDataActivationStatus::LargeKernelInQtResource)
    );
    QVERIFY(!result.diagnostics.empty());
}

void EphemerisDataActivationTests::verifiesCompleteStagedUpdateSet()
{
    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    const skygate::ephemeris::EphemerisDataManifest manifest = makeStagedManifest();
    writeAllStagedAssets(stagedRoot, manifest);

    const skygate::ephemeris::EphemerisStagedUpdateVerificationResult result =
        skygate::ephemeris::verifyEphemerisStagedUpdateSet(verificationRequest(manifest, stagedRoot));

    QVERIFY(result.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisStagedUpdateVerificationStatus::Verified)
    );
    QCOMPARE(result.verifiedAssetIds.size(), std::size_t{4});
}

void EphemerisDataActivationTests::cancelsStagedUpdateVerification()
{
    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    const skygate::ephemeris::EphemerisDataManifest manifest = makeStagedManifest();
    writeAllStagedAssets(stagedRoot, manifest);
    skygate::ephemeris::EphemerisStagedUpdateVerificationRequest request = verificationRequest(manifest, stagedRoot);
    request.cancellationRequested = [] { return true; };

    const skygate::ephemeris::EphemerisStagedUpdateVerificationResult result =
        skygate::ephemeris::verifyEphemerisStagedUpdateSet(request);

    QVERIFY(!result.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisStagedUpdateVerificationStatus::Canceled)
    );
    QVERIFY(!result.diagnostics.empty());
    QVERIFY(result.verifiedAssetIds.empty());
}

void EphemerisDataActivationTests::rejectsStagedUpdateChecksumFailure()
{
    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    skygate::ephemeris::EphemerisDataManifest manifest = makeStagedManifest();
    manifest.assets[1].checksum.value = std::string(64U, '0');
    writeAllStagedAssets(stagedRoot, manifest);

    const skygate::ephemeris::EphemerisStagedUpdateVerificationResult result =
        skygate::ephemeris::verifyEphemerisStagedUpdateSet(verificationRequest(manifest, stagedRoot));

    QVERIFY(!result.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisStagedUpdateVerificationStatus::ChecksumMismatch)
    );
    QVERIFY(!result.diagnostics.empty());
}

void EphemerisDataActivationTests::rejectsStagedUpdateWrongComponentKind()
{
    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    skygate::ephemeris::EphemerisDataManifest manifest = makeStagedManifest();
    manifest.assets[0].kind = skygate::ephemeris::EphemerisDataManifestAssetKind::EarthOrientationData;
    writeAllStagedAssets(stagedRoot, manifest);

    const skygate::ephemeris::EphemerisStagedUpdateVerificationResult result =
        skygate::ephemeris::verifyEphemerisStagedUpdateSet(verificationRequest(manifest, stagedRoot));

    QVERIFY(!result.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisStagedUpdateVerificationStatus::WrongComponentKind)
    );
    QVERIFY(!result.diagnostics.empty());
}

void EphemerisDataActivationTests::rejectsStagedUpdateVersionMismatch()
{
    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    skygate::ephemeris::EphemerisDataManifest manifest = makeStagedManifest();
    manifest.assets[1].version = "stale";
    writeAllStagedAssets(stagedRoot, manifest);

    const skygate::ephemeris::EphemerisStagedUpdateVerificationResult result =
        skygate::ephemeris::verifyEphemerisStagedUpdateSet(verificationRequest(manifest, stagedRoot));

    QVERIFY(!result.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisStagedUpdateVerificationStatus::MismatchedMetadata)
    );
    QVERIFY(!result.diagnostics.empty());
}

void EphemerisDataActivationTests::rejectsStagedUpdateValidityRangeMismatch()
{
    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    skygate::ephemeris::EphemerisDataManifest manifest = makeStagedManifest();
    writeAllStagedAssets(stagedRoot, manifest);
    skygate::ephemeris::EphemerisStagedUpdateVerificationRequest request = verificationRequest(manifest, stagedRoot);
    request.expectedComponents[0].requiredValidityRange = makeValidityRange(1990, 2100);

    const skygate::ephemeris::EphemerisStagedUpdateVerificationResult result =
        skygate::ephemeris::verifyEphemerisStagedUpdateSet(request);

    QVERIFY(!result.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisStagedUpdateVerificationStatus::MismatchedMetadata)
    );
    QVERIFY(!result.diagnostics.empty());
}

void EphemerisDataActivationTests::rejectsIncompleteStagedUpdateSet()
{
    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    skygate::ephemeris::EphemerisDataManifest manifest = makeStagedManifest();
    writeAllStagedAssets(stagedRoot, manifest);
    QVERIFY(QFile::remove(stagedRoot.path() + QStringLiteral("/time/eop.csv")));

    const skygate::ephemeris::EphemerisStagedUpdateVerificationResult result =
        skygate::ephemeris::verifyEphemerisStagedUpdateSet(verificationRequest(manifest, stagedRoot));

    QVERIFY(!result.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisStagedUpdateVerificationStatus::MissingAsset)
    );
    QVERIFY(!result.diagnostics.empty());
}

void EphemerisDataActivationTests::rejectsMalformedStagedMetadata()
{
    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    skygate::ephemeris::EphemerisDataManifest manifest = makeStagedManifest();
    manifest.assets[0].relativePath = "../kernel.bsp";

    const skygate::ephemeris::EphemerisStagedUpdateVerificationResult result =
        skygate::ephemeris::verifyEphemerisStagedUpdateSet(verificationRequest(manifest, stagedRoot));

    QVERIFY(!result.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisStagedUpdateVerificationStatus::MalformedMetadata)
    );
    QVERIFY(!result.diagnostics.empty());
}

void EphemerisDataActivationTests::rejectsMalformedStagedValidityRangeLabels()
{
    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    skygate::ephemeris::EphemerisDataManifest manifest = makeStagedManifest();
    manifest.assets[0].validityRange.displayName.clear();

    const skygate::ephemeris::EphemerisStagedUpdateVerificationResult result =
        skygate::ephemeris::verifyEphemerisStagedUpdateSet(verificationRequest(manifest, stagedRoot));

    QVERIFY(!result.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisStagedUpdateVerificationStatus::MalformedMetadata)
    );
    QVERIFY(!result.diagnostics.empty());
}

void EphemerisDataActivationTests::rejectsCorruptCompressedStagedAsset()
{
    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    skygate::ephemeris::EphemerisDataManifest manifest = makeStagedManifest();
    manifest.assets[0] = makeZstdAsset();
    manifest.assets[0].validityRange = testValidityRange();
    manifest.assets[0].compression.compressedSizeBytes = QByteArrayLiteral("not-zstd").size();
    writeAllStagedAssets(stagedRoot, manifest);
    writeFile(stagedRoot.path() + QStringLiteral("/kernels/de440s.bsp.zst"), QByteArrayLiteral("not-zstd"));

    const skygate::ephemeris::EphemerisStagedUpdateVerificationResult result =
        skygate::ephemeris::verifyEphemerisStagedUpdateSet(verificationRequest(manifest, stagedRoot));

    if (result.status == skygate::ephemeris::EphemerisStagedUpdateVerificationStatus::UnsupportedCompression) {
        QSKIP("zstd runtime is not available in this test environment.");
    }
    QVERIFY(!result.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisStagedUpdateVerificationStatus::CorruptArchive)
    );
    QVERIFY(!result.diagnostics.empty());
}

void EphemerisDataActivationTests::rejectsUnsupportedStagedProfile()
{
    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    const skygate::ephemeris::EphemerisDataManifest manifest = makeStagedManifest();
    skygate::ephemeris::EphemerisStagedUpdateVerificationRequest request = verificationRequest(manifest, stagedRoot);
    request.profileId = "missing-profile";

    const skygate::ephemeris::EphemerisStagedUpdateVerificationResult result =
        skygate::ephemeris::verifyEphemerisStagedUpdateSet(request);

    QVERIFY(!result.isSuccess());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisStagedUpdateVerificationStatus::UnsupportedProfile)
    );
    QVERIFY(!result.diagnostics.empty());
}

QTEST_APPLESS_MAIN(EphemerisDataActivationTests)

#include "EphemerisDataActivationTests.moc"
