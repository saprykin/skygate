#include "CatalogCacheTestSupport.hpp"
#include "SettingsTestFixture.hpp"
#include "SkyEphemerisDataManager.hpp"
#include "SkySettingsStore.hpp"

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <cstdint>
#include <string>
#include <string_view>

namespace {

constexpr std::string_view kPayload = "SkyGate acceptance ephemeris update\n";
constexpr std::string_view kPayloadSha256 = "bc12fcf669402a46ba2bd1b56aa2392d93e79ef535dd79ae056e4222c0410ffd";

bool writeFile(const QString& path, const QByteArray& contents)
{
    const QFileInfo fileInfo(path);
    if (!QDir().mkpath(fileInfo.absolutePath())) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }

    return file.write(contents) == contents.size();
}

skygate::ephemeris::EphemerisDateRange acceptanceRange()
{
    const auto start = skygate::ephemeris::astronomicalEpochFromCivilDateTime(skygate::ephemeris::CivilDateTime{
        .astronomicalYear = 2000,
        .month = 1,
        .day = 1,
        .timeScale = skygate::ephemeris::TimeScale::Utc,
    });
    const auto end = skygate::ephemeris::astronomicalEpochFromCivilDateTime(skygate::ephemeris::CivilDateTime{
        .astronomicalYear = 2050,
        .month = 1,
        .day = 1,
        .timeScale = skygate::ephemeris::TimeScale::Utc,
    });
    Q_ASSERT(start.has_value());
    Q_ASSERT(end.has_value());
    return skygate::ephemeris::EphemerisDateRange{
        .id = "acceptance-modern",
        .displayName = "Acceptance modern range",
        .start = *start,
        .end = *end,
    };
}

skygate::ephemeris::EphemerisDataManifest acceptanceManifest()
{
    skygate::ephemeris::EphemerisDataManifest manifest;
    manifest.dataSetInfo.id = "acceptance-data";
    manifest.dataSetInfo.displayName = "Acceptance data";
    manifest.dataSetInfo.version = "2026a";
    manifest.dataSetInfo.provenance = "acceptance test";
    manifest.dataSetInfo.dateRanges.push_back(acceptanceRange());
    manifest.profiles.push_back(skygate::ephemeris::EphemerisDataManifestProfile{
        .id = "modern",
        .displayName = "Modern",
        .bundled = false,
        .longRange = false,
        .assetIds = {"de440s-kernel"},
    });
    manifest.assets.push_back(skygate::ephemeris::EphemerisDataManifestAsset{
        .id = "de440s-kernel",
        .kind = skygate::ephemeris::EphemerisDataManifestAssetKind::SolarSystemKernel,
        .profileId = "modern",
        .version = "acceptance-kernel",
        .sourceUrl = "https://example.invalid/de440s.bsp",
        .relativePath = "kernels/de440s.bsp",
        .checksum = {.algorithm = "sha256", .value = std::string{kPayloadSha256}},
        .compression =
            {
                .kind = skygate::ephemeris::EphemerisDataManifestCompressionKind::None,
                .uncompressedSizeBytes = kPayload.size(),
            },
        .validityRange = acceptanceRange(),
        .optional = false,
    });
    return manifest;
}

void writeStagedAssets(const QTemporaryDir& root, const skygate::ephemeris::EphemerisDataManifest& manifest)
{
    const QByteArray payload(kPayload.data(), static_cast<qsizetype>(kPayload.size()));
    for (const skygate::ephemeris::EphemerisDataManifestAsset& asset : manifest.assets) {
        QVERIFY(writeFile(root.path() + QStringLiteral("/") + QString::fromStdString(asset.relativePath), payload));
    }
}

SkyEphemerisDataManager::StagedUpdateActivationRequest activationRequest(
    const skygate::ephemeris::EphemerisDataManifest& manifest,
    const QTemporaryDir& stagedRoot,
    const QString& writableCacheRoot
)
{
    SkyEphemerisDataManager::StagedUpdateActivationRequest request;
    request.manifest = &manifest;
    request.profileId = QStringLiteral("modern");
    request.stagedResourceRoot = stagedRoot.path();
    request.writableCacheRoot = writableCacheRoot;
    request.revisionToken = QStringLiteral("acceptance-rev");
    request.requiredKinds = {skygate::ephemeris::EphemerisDataManifestAssetKind::SolarSystemKernel};
    request.expectedComponents = {
        {"de440s-kernel", skygate::ephemeris::EphemerisDataManifestAssetKind::SolarSystemKernel},
    };
    request.expectedComponents.front().expectedVersion = "acceptance-kernel";
    request.expectedComponents.front().requiredValidityRange = acceptanceRange();
    return request;
}

void compareCatalogCacheSnapshots(
    const SkySettingsStore::CatalogCacheSnapshot& actual, const SkySettingsStore::CatalogCacheSnapshot& expected
)
{
    QCOMPARE(actual.sourceLabel, expected.sourceLabel);
    QCOMPARE(actual.catalogPayload, expected.catalogPayload);
    QCOMPARE(actual.deepSkySourceLabel, expected.deepSkySourceLabel);
    QCOMPARE(actual.deepSkyCatalogPayload, expected.deepSkyCatalogPayload);
    QCOMPARE(actual.constellationLineRows, expected.constellationLineRows);
    QCOMPARE(actual.constellationLabelRows, expected.constellationLabelRows);
    QCOMPARE(actual.constellationLineSchemaVersion, expected.constellationLineSchemaVersion);
    QCOMPARE(actual.constellationCount, expected.constellationCount);
}

}  // namespace

class SkyAcceptanceMatrixTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void engineSelectionPersistsAcrossRestart();
    void ephemerisDataUpdateLeavesCatalogStateUnchanged();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyAcceptanceMatrixTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyAcceptanceMatrixTests")));
}

void SkyAcceptanceMatrixTests::init()
{
    m_settings.resetForCurrentTest();
}

void SkyAcceptanceMatrixTests::engineSelectionPersistsAcrossRestart()
{
    SkySettingsStore::StateSnapshot savedSnapshot;
    savedSnapshot.ephemeris.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    savedSnapshot.ephemeris.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::ApparentTopocentric;
    savedSnapshot.ephemeris.correctionPresetId = QStringLiteral("apparent-topocentric");
    savedSnapshot.ephemeris.preferredDataProfileId = QStringLiteral("de441-long-range");
    savedSnapshot.ephemerisSettingsPresent = true;

    SkySettingsStore firstStore;
    QVERIFY(firstStore.saveState(savedSnapshot));

    SkySettingsStore restartedStore;
    const auto restoredSnapshot = restartedStore.loadState();
    QVERIFY(restoredSnapshot.has_value());
    QVERIFY(restoredSnapshot->ephemerisSettingsPresent);
    QCOMPARE(
        static_cast<std::uint8_t>(restoredSnapshot->ephemeris.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(restoredSnapshot->ephemeris.correctionFlags),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::ApparentTopocentric)
    );
    QCOMPARE(restoredSnapshot->ephemeris.correctionPresetId, QString("apparent-topocentric"));
    QCOMPARE(restoredSnapshot->ephemeris.preferredDataProfileId, QString("de441-long-range"));
}

void SkyAcceptanceMatrixTests::ephemerisDataUpdateLeavesCatalogStateUnchanged()
{
    SkySettingsStore store;
    const SkySettingsStore::CatalogCacheSnapshot catalogSnapshot = skygate::ui::tests::sampleCatalogCacheSnapshot();
    QVERIFY(store.saveCatalogCache(catalogSnapshot));

    const skygate::ephemeris::EphemerisDataManifest manifest = acceptanceManifest();
    QTemporaryDir stagedRoot;
    QVERIFY(stagedRoot.isValid());
    writeStagedAssets(stagedRoot, manifest);

    SkyEphemerisDataManager manager(&store);
    const SkyEphemerisDataManager::StagedUpdateActivationResult result =
        manager.activateVerifiedStagedUpdateSet(activationRequest(manifest, stagedRoot, m_settings.path()));

    const QByteArray failureMessage = result.diagnostics.empty() ? QByteArray{} : result.diagnostics.front().toUtf8();
    QVERIFY2(result.isSuccess(), failureMessage.constData());
    QCOMPARE(manager.dataRevisionToken(), QString("acceptance-rev"));
    QCOMPARE(result.activatedAssetIds.size(), std::size_t{1});

    const auto loadedCatalogSnapshot = store.loadCatalogCache();
    QVERIFY(loadedCatalogSnapshot.has_value());
    compareCatalogCacheSnapshots(*loadedCatalogSnapshot, catalogSnapshot);
}

QTEST_GUILESS_MAIN(SkyAcceptanceMatrixTests)

#include "SkyAcceptanceMatrixTests.moc"
