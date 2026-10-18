#include "engine/EphemerisDataManifest.hpp"
#include "ephemeris/SkyEphemerisAssetStatus.hpp"
#include "ephemeris/SkyEphemerisDataCacheSnapshotPolicy.hpp"
#include "ephemeris/SkyEphemerisDataManager.hpp"
#include "ephemeris/SkyEphemerisDataProfileIds.hpp"
#include "settings/SkySettingsStore.hpp"
#include "support/EphemerisDataCacheSnapshotFixture.hpp"
#include "support/SettingsTestFixture.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QString>
#include <QtTest/QtTest>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

using skygate::ui::internal::SkyEphemerisAssetStatus;
using skygate::ui::internal::SkyEphemerisDataCacheSnapshotPolicy;
using skygate::ui::internal::SkyEphemerisDataProfileIds;
using skygate::ui::tests::EphemerisDataCacheSnapshotFixture;
using skygate::ui::tests::SettingsTestFixture;

namespace {

#ifndef SKYGATE_UI_SOURCE_DIR
#define SKYGATE_UI_SOURCE_DIR ""
#endif

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

bool isSha256Hex(const std::string& value)
{
    if (value.size() != 64U) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](const char character) {
        return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
    });
}

skygate::ephemeris::EphemerisDataManifest loadProductionManifest()
{
    const QString manifestPath =
        QStringLiteral(SKYGATE_UI_SOURCE_DIR) + QStringLiteral("/resources/ephemeris/manifest.json");
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTest::qFail(
            qPrintable(QStringLiteral("Unable to open production manifest: %1").arg(manifestPath)), __FILE__, __LINE__
        );
        return {};
    }

    const QByteArray payload = manifestFile.readAll();
    skygate::ephemeris::EphemerisDataManifest::ParseResult result = skygate::ephemeris::EphemerisDataManifest::parse(
        std::string_view(payload.constData(), static_cast<std::size_t>(payload.size()))
    );
    if (!result.isSuccess()) {
        const QString diagnostic =
            result.diagnostics.empty() ? QStringLiteral("unknown") : QString::fromStdString(result.diagnostics.front());
        QTest::qFail(
            qPrintable(QStringLiteral("Production manifest did not parse: %1").arg(diagnostic)), __FILE__, __LINE__
        );
        return {};
    }

    return std::move(result.manifest);
}

}  // namespace

class SkyEphemerisDataStatusTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void initialBundledStatus();
    void productionManifestMetadataIsVerifiable();
    void installedDataStatusAndSnapshot();
    void legacyShortRangeProfileIdRestoresAsDe440sProfile();
    void missingInstalledDataFallsBackToBundled();
    void unbundledProductionOmitsMissingFallbackFiles();
    void revisionSignalEmitsOnlyWhenActiveDataChanges();
    void kernelDisplayTextForBundledData();
    void kernelDisplayTextForMissingInstalledData();
    void kernelDisplayTextForInstalledKernels();
    void kernelDisplayTextForRecordedKernelRange();
    void kernelDisplayTextForLegacyProfile();
    void kernelDisplayTextForUnexpectedMetadata();
    void normalizedSnapshotTrimsAndDefaults();
    void profileIdsCanonicalizeLegacyAlias();
    void assetStatusDecodesManagerText();

private:
    SettingsTestFixture m_settings;
};

void SkyEphemerisDataStatusTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyEphemerisDataStatusTests")));
}

void SkyEphemerisDataStatusTests::init()
{
    m_settings.clearSettings();
}

void SkyEphemerisDataStatusTests::initialBundledStatus()
{
    SkySettingsStore store;
    SkyEphemerisDataManager manager(&store);

    QVERIFY(!manager.usingInstalledData());
    QCOMPARE(manager.dataRevisionToken(), QString("bundled"));
    QCOMPARE(manager.statusText(), QString("Ephemeris data: Bundled fallback"));
    QCOMPARE(manager.datasetInfoText(), QString("Bundled fallback"));
    QVERIFY(manager.activeDataSnapshot() != nullptr);
    QVERIFY(!manager.activeDataSnapshot()->earthOrientationDataAsset().has_value());
}

void SkyEphemerisDataStatusTests::productionManifestMetadataIsVerifiable()
{
    const skygate::ephemeris::EphemerisDataManifest manifest = loadProductionManifest();
    QVERIFY(!manifest.profiles.empty());
    QVERIFY(!manifest.assets.empty());

    for (const skygate::ephemeris::EphemerisDataManifest::Asset& asset : manifest.assets) {
        QVERIFY2(!asset.sourceUrl.empty(), asset.id.c_str());
        if (asset.live) {
            QVERIFY2(asset.checksum.algorithm.empty() && asset.checksum.value.empty(), asset.id.c_str());
            QVERIFY2(!asset.compression.uncompressedSizeBytes.has_value(), asset.id.c_str());
        } else {
            QCOMPARE(QString::fromStdString(asset.checksum.algorithm), QString("sha256"));
            QVERIFY2(isSha256Hex(asset.checksum.value), asset.id.c_str());
            QVERIFY2(asset.compression.uncompressedSizeBytes.has_value(), asset.id.c_str());
            QVERIFY2(*asset.compression.uncompressedSizeBytes > 0U, asset.id.c_str());
        }
    }

    for (const skygate::ephemeris::EphemerisDataManifest::Profile& profile : manifest.profiles) {
        for (const std::string& assetId : profile.assetIds) {
            const skygate::ephemeris::EphemerisDataManifest::Asset* asset = manifest.asset(assetId);
            QVERIFY2(asset != nullptr, assetId.c_str());
            QCOMPARE(QString::fromStdString(asset->profileId), QString::fromStdString(profile.id));
        }
    }

    for (const std::string_view profileId :
         {std::string_view{"de440s-short-range"}, std::string_view{"de441-long-range"}}) {
        const skygate::ephemeris::EphemerisDataManifest::Profile* profile = manifest.profile(profileId);
        QVERIFY2(profile != nullptr, profileId.data());
        QCOMPARE(profile->assetIds.size(), std::size_t{1});

        const skygate::ephemeris::EphemerisDataManifest::Asset* asset = manifest.asset(profile->assetIds.front());
        QVERIFY2(asset != nullptr, profileId.data());
        QCOMPARE(
            static_cast<std::uint8_t>(asset->kind),
            static_cast<std::uint8_t>(skygate::ephemeris::EphemerisDataManifest::AssetKind::SolarSystemKernel)
        );
    }
}

void SkyEphemerisDataStatusTests::installedDataStatusAndSnapshot()
{
    const QString kernelPath = m_settings.filePath(QStringLiteral("kernel.bsp"));
    const QString earthOrientationPath = m_settings.filePath(QStringLiteral("eop.txt"));
    const QString leapSecondTablePath = m_settings.filePath(QStringLiteral("leap-seconds.list"));
    const QString deltaTDataPath = m_settings.filePath(QStringLiteral("delta-t.csv"));
    QVERIFY(writeFile(kernelPath, QByteArrayLiteral("kernel placeholder")));
    QVERIFY(writeFile(earthOrientationPath, QByteArrayLiteral("eop payload")));
    QVERIFY(writeFile(leapSecondTablePath, QByteArrayLiteral("leap payload")));
    QVERIFY(writeFile(deltaTDataPath, QByteArrayLiteral("delta t payload")));

    SkySettingsStore store;
    QVERIFY(store.saveEphemerisDataCache(
        EphemerisDataCacheSnapshotFixture::installedSnapshot(
            kernelPath, earthOrientationPath, leapSecondTablePath, deltaTDataPath
        )
    ));

    SkyEphemerisDataManager manager(&store);
    QVERIFY(manager.usingInstalledData());
    QCOMPARE(manager.statusText(), QString("Ephemeris data: Installed data active"));
    QCOMPARE(manager.dataRevisionToken(), QString("installed-rev"));
    QVERIFY(manager.datasetInfoText().contains(QStringLiteral("Kernel DE-test")));
    QVERIFY(manager.datasetInfoText().contains(QStringLiteral("EOP EOP-test")));

    const auto snapshot = manager.activeDataSnapshot();
    QVERIFY(snapshot != nullptr);
    const auto kernelAsset = snapshot->solarSystemKernelAsset("de440s-kernel");
    QVERIFY(kernelAsset.has_value());
    QCOMPARE(QString::fromStdString(kernelAsset->id), QString("de440s-kernel"));
    QCOMPARE(QString::fromStdString(kernelAsset->profileId), QString("de440s-short-range"));
    QCOMPARE(QString::fromStdString(kernelAsset->activePath), kernelPath);
    QVERIFY(!snapshot->solarSystemKernelAsset("de441-kernel").has_value());

    const auto eopAsset = snapshot->earthOrientationDataAsset();
    QVERIFY(eopAsset.has_value());
    QCOMPARE(QString::fromStdString(eopAsset->content), QString("eop payload"));
    const auto leapSecondAsset = snapshot->leapSecondTableAsset();
    QVERIFY(leapSecondAsset.has_value());
    QCOMPARE(QString::fromStdString(leapSecondAsset->content), QString("leap payload"));
    const auto deltaTAsset = snapshot->deltaTDataAsset();
    QVERIFY(deltaTAsset.has_value());
    QCOMPARE(QString::fromStdString(deltaTAsset->content), QString("delta t payload"));
}

void SkyEphemerisDataStatusTests::legacyShortRangeProfileIdRestoresAsDe440sProfile()
{
    const QString kernelPath = m_settings.filePath(QStringLiteral("legacy-de440s.bsp"));
    QVERIFY(writeFile(kernelPath, QByteArrayLiteral("kernel placeholder")));

    SkyEphemerisDataCacheSnapshot snapshot =
        EphemerisDataCacheSnapshotFixture::installedSnapshot(kernelPath, QString());
    snapshot.installedKernelProfileId = QStringLiteral("modern");

    SkySettingsStore store;
    QVERIFY(store.saveEphemerisDataCache(snapshot));

    SkyEphemerisDataManager manager(&store);
    QVERIFY(manager.usingInstalledData());
    QCOMPARE(manager.activeCacheSnapshot().installedKernelProfileId, QString("de440s-short-range"));

    const auto activeSnapshot = manager.activeDataSnapshot();
    QVERIFY(activeSnapshot != nullptr);
    const auto kernelAsset = activeSnapshot->solarSystemKernelAsset("de440s-kernel");
    QVERIFY(kernelAsset.has_value());
    QCOMPARE(QString::fromStdString(kernelAsset->profileId), QString("de440s-short-range"));
}

void SkyEphemerisDataStatusTests::missingInstalledDataFallsBackToBundled()
{
    SkySettingsStore store;
    QVERIFY(store.saveEphemerisDataCache(
        EphemerisDataCacheSnapshotFixture::installedSnapshot(
            m_settings.filePath(QStringLiteral("missing-kernel.bsp")), QString()
        )
    ));

    SkyEphemerisDataManager manager(&store);
    QVERIFY(!manager.usingInstalledData());
    QCOMPARE(manager.dataRevisionToken(), QString("bundled"));
    QVERIFY(manager.statusText().contains(QStringLiteral("Installed data missing")));
    QVERIFY(manager.datasetInfoText().contains(QStringLiteral("Bundled fallback")));
    QVERIFY(manager.activeDataSnapshot() != nullptr);
}

void SkyEphemerisDataStatusTests::unbundledProductionOmitsMissingFallbackFiles()
{
    const skygate::ephemeris::EphemerisDataManifest manifest = loadProductionManifest();
    SkySettingsStore store;
    SkyEphemerisDataManager manager(&store);

    manager.setBundledFallbackData(&manifest, QStringLiteral(":/ephemeris"), QStringLiteral("de440s-short-range"));

    const auto snapshot = manager.activeDataSnapshot();
    QVERIFY(snapshot != nullptr);
    QVERIFY(!snapshot->solarSystemKernelAsset("de440s-kernel").has_value());
    QVERIFY(!snapshot->solarSystemKernelAsset("de441-kernel").has_value());
}

void SkyEphemerisDataStatusTests::kernelDisplayTextForBundledData()
{
    SkySettingsStore store;
    SkyEphemerisDataManager manager(&store);

    QVERIFY(!manager.usingInstalledData());
    QCOMPARE(manager.kernelDisplayText(), QString("Bundled"));
}

void SkyEphemerisDataStatusTests::kernelDisplayTextForMissingInstalledData()
{
    SkySettingsStore store;
    QVERIFY(store.saveEphemerisDataCache(
        EphemerisDataCacheSnapshotFixture::installedSnapshot(
            m_settings.filePath(QStringLiteral("display-missing-kernel.bsp")), QString()
        )
    ));

    SkyEphemerisDataManager manager(&store);

    QVERIFY(!manager.usingInstalledData());
    QCOMPARE(manager.kernelDisplayText(), QString("Bundled"));
}

void SkyEphemerisDataStatusTests::kernelDisplayTextForInstalledKernels()
{
    const QString de440sPath = m_settings.filePath(QStringLiteral("display-de440s.bsp"));
    QVERIFY(writeFile(de440sPath, QByteArrayLiteral("kernel")));
    SkySettingsStore store;
    QVERIFY(store.saveEphemerisDataCache(EphemerisDataCacheSnapshotFixture::installedSnapshot(de440sPath, QString())));
    {
        SkyEphemerisDataManager manager(&store);
        QVERIFY(manager.usingInstalledData());
        QCOMPARE(manager.kernelDisplayText(), QString("DE440s"));
    }

    SkyEphemerisDataCacheSnapshot de441Snapshot = EphemerisDataCacheSnapshotFixture::installedSnapshot(
        m_settings.filePath(QStringLiteral("display-de441.bsp")), QString()
    );
    QVERIFY(writeFile(de441Snapshot.installedKernelPath, QByteArrayLiteral("kernel")));
    de441Snapshot.installedKernelAssetId = QStringLiteral("de441-kernel");
    de441Snapshot.installedKernelProfileId = QStringLiteral("de441-long-range");
    de441Snapshot.installedKernelVersion = QStringLiteral("DE441-test");
    QVERIFY(store.saveEphemerisDataCache(de441Snapshot));

    SkyEphemerisDataManager manager(&store);

    QVERIFY(manager.usingInstalledData());
    QCOMPARE(manager.kernelDisplayText(), QString("DE441"));
}

void SkyEphemerisDataStatusTests::kernelDisplayTextForRecordedKernelRange()
{
    const QString kernelPath = m_settings.filePath(QStringLiteral("display-recorded-range.bsp"));
    QVERIFY(writeFile(kernelPath, QByteArrayLiteral("kernel")));

    SkyEphemerisDataCacheSnapshot snapshot =
        EphemerisDataCacheSnapshotFixture::installedSnapshot(kernelPath, QString());
    // No id or version names a kernel model here, so only the recorded range
    // can decide the display text.
    snapshot.installedKernelAssetId = QStringLiteral("custom-kernel");
    snapshot.installedKernelProfileId = QStringLiteral("custom-profile");
    snapshot.installedKernelVersion = QStringLiteral("custom-version");
    snapshot.installedKernelLongRange = true;

    SkySettingsStore store;
    QVERIFY(store.saveEphemerisDataCache(snapshot));
    {
        const SkyEphemerisDataCacheSnapshot restored = store.loadEphemerisDataCache();
        QVERIFY(restored.installedKernelLongRange.has_value());
        QCOMPARE(restored.installedKernelLongRange.value(), true);

        SkyEphemerisDataManager manager(&store);
        QVERIFY(manager.usingInstalledData());
        QCOMPARE(manager.kernelDisplayText(), QString("DE441"));
    }

    snapshot.installedKernelLongRange = false;
    QVERIFY(store.saveEphemerisDataCache(snapshot));
    {
        const SkyEphemerisDataCacheSnapshot restored = store.loadEphemerisDataCache();
        QVERIFY(restored.installedKernelLongRange.has_value());
        QCOMPARE(restored.installedKernelLongRange.value(), false);

        SkyEphemerisDataManager manager(&store);
        QVERIFY(manager.usingInstalledData());
        QCOMPARE(manager.kernelDisplayText(), QString("DE440s"));
    }
}

void SkyEphemerisDataStatusTests::kernelDisplayTextForLegacyProfile()
{
    const QString kernelPath = m_settings.filePath(QStringLiteral("display-legacy-modern.bsp"));
    QVERIFY(writeFile(kernelPath, QByteArrayLiteral("kernel")));
    SkyEphemerisDataCacheSnapshot snapshot =
        EphemerisDataCacheSnapshotFixture::installedSnapshot(kernelPath, QString());
    snapshot.installedKernelProfileId = QStringLiteral("modern");
    snapshot.installedKernelVersion = QStringLiteral("DE440s-2026a");

    SkySettingsStore store;
    QVERIFY(store.saveEphemerisDataCache(snapshot));

    SkyEphemerisDataManager manager(&store);

    QVERIFY(manager.usingInstalledData());
    QCOMPARE(manager.activeCacheSnapshot().installedKernelProfileId, QString("de440s-short-range"));
    QCOMPARE(manager.kernelDisplayText(), QString("DE440s"));
}

void SkyEphemerisDataStatusTests::kernelDisplayTextForUnexpectedMetadata()
{
    const QString kernelPath = m_settings.filePath(QStringLiteral("display-unexpected-kernel.bsp"));
    QVERIFY(writeFile(kernelPath, QByteArrayLiteral("kernel")));
    SkyEphemerisDataCacheSnapshot snapshot =
        EphemerisDataCacheSnapshotFixture::installedSnapshot(kernelPath, QString());
    snapshot.installedKernelAssetId = QStringLiteral("custom-kernel");
    snapshot.installedKernelProfileId = QStringLiteral("custom-profile");
    snapshot.installedKernelVersion = QStringLiteral("custom-version");

    SkySettingsStore store;
    QVERIFY(store.saveEphemerisDataCache(snapshot));

    SkyEphemerisDataManager manager(&store);

    QVERIFY(manager.usingInstalledData());
    QCOMPARE(manager.shortRangeKernelStatusText(), QString("Installed: custom-version"));
    QCOMPARE(manager.longRangeKernelStatusText(), QString("Not installed"));
    QCOMPARE(manager.kernelDisplayText(), QString("Bundled"));
}

void SkyEphemerisDataStatusTests::revisionSignalEmitsOnlyWhenActiveDataChanges()
{
    SkySettingsStore store;
    SkyEphemerisDataManager manager(&store);
    const std::uint64_t originalRevision = manager.dataRevision();
    QSignalSpy activeDataSpy(&manager, &SkyEphemerisDataManager::activeDataChanged);
    QSignalSpy revisionSpy(&manager, &SkyEphemerisDataManager::dataRevisionChanged);

    const QString kernelPath = m_settings.filePath(QStringLiteral("revision-kernel.bsp"));
    QVERIFY(writeFile(kernelPath, QByteArrayLiteral("kernel")));
    QVERIFY(store.saveEphemerisDataCache(EphemerisDataCacheSnapshotFixture::installedSnapshot(kernelPath, QString())));

    QVERIFY(manager.restoreFromSettings());
    QCOMPARE(activeDataSpy.count(), 1);
    QCOMPARE(revisionSpy.count(), 1);
    QVERIFY(manager.dataRevision() > originalRevision);

    QVERIFY(manager.restoreFromSettings());
    QCOMPARE(activeDataSpy.count(), 1);
    QCOMPARE(revisionSpy.count(), 1);
}

void SkyEphemerisDataStatusTests::normalizedSnapshotTrimsAndDefaults()
{
    SkyEphemerisDataCacheSnapshot snapshot;
    snapshot.installedKernelAssetId = QStringLiteral("  de440s-kernel  ");
    snapshot.installedKernelProfileId = QStringLiteral(" modern ");
    snapshot.installedKernelPath = QStringLiteral("  /tmp/de440s.bsp  ");
    snapshot.installedKernelVersion = QStringLiteral("  DE440s-2026a  ");
    snapshot.installedEarthOrientationPath = QStringLiteral("  /tmp/eop.txt  ");
    snapshot.installedEarthOrientationVersion = QStringLiteral("  2026-05-07  ");
    snapshot.installedLeapSecondTablePath = QStringLiteral("  /tmp/leap-seconds.list  ");
    snapshot.installedLeapSecondTableVersion = QStringLiteral("  2026a  ");
    snapshot.installedDeltaTDataPath = QStringLiteral("  /tmp/delta-t.csv  ");
    snapshot.installedDeltaTDataVersion = QStringLiteral("  2026a  ");
    snapshot.dataRevisionToken = QStringLiteral("   ");
    snapshot.lastUpdateResult = QStringLiteral("  ");

    const SkyEphemerisDataCacheSnapshot normalized = SkyEphemerisDataCacheSnapshotPolicy::normalizedSnapshot(snapshot);

    QCOMPARE(normalized.installedKernelAssetId, QString("de440s-kernel"));
    QCOMPARE(normalized.installedKernelProfileId, QString("de440s-short-range"));
    QCOMPARE(normalized.installedKernelPath, QString("/tmp/de440s.bsp"));
    QCOMPARE(normalized.installedKernelVersion, QString("DE440s-2026a"));
    QCOMPARE(normalized.installedEarthOrientationPath, QString("/tmp/eop.txt"));
    QCOMPARE(normalized.installedEarthOrientationVersion, QString("2026-05-07"));
    QCOMPARE(normalized.installedLeapSecondTablePath, QString("/tmp/leap-seconds.list"));
    QCOMPARE(normalized.installedLeapSecondTableVersion, QString("2026a"));
    QCOMPARE(normalized.installedDeltaTDataPath, QString("/tmp/delta-t.csv"));
    QCOMPARE(normalized.installedDeltaTDataVersion, QString("2026a"));
    QCOMPARE(normalized.dataRevisionToken, QString("bundled"));
    QCOMPARE(normalized.lastUpdateResult, QString("Bundled fallback"));
}

void SkyEphemerisDataStatusTests::profileIdsCanonicalizeLegacyAlias()
{
    QCOMPARE(SkyEphemerisDataProfileIds::canonical(QStringLiteral("modern")), QString("de440s-short-range"));
    QCOMPARE(SkyEphemerisDataProfileIds::canonical(QStringLiteral("  modern  ")), QString("de440s-short-range"));
    QCOMPARE(SkyEphemerisDataProfileIds::canonical(QStringLiteral("de441-long-range")), QString("de441-long-range"));
    QCOMPARE(SkyEphemerisDataProfileIds::canonical(QString()), QString());
    QCOMPARE(QString(SkyEphemerisDataProfileIds::kDefaultProfileId), QString("de440s-short-range"));
}

void SkyEphemerisDataStatusTests::assetStatusDecodesManagerText()
{
    const SkyEphemerisAssetStatus bundled = SkyEphemerisAssetStatus::fromStatusText(QStringLiteral("Bundled fallback"));
    QCOMPARE(
        static_cast<std::uint8_t>(bundled.source()),
        static_cast<std::uint8_t>(SkyEphemerisAssetStatus::Source::BundledFallback)
    );
    QVERIFY(!bundled.isInstalled());
    QVERIFY(bundled.version().isEmpty());
    QCOMPARE(bundled.displayText(), QString("Bundled"));
    QCOMPARE(bundled.statusText(), QString("Bundled fallback"));

    const SkyEphemerisAssetStatus missing =
        SkyEphemerisAssetStatus::fromStatusText(QStringLiteral("Bundled fallback (installed data missing)"));
    QCOMPARE(
        static_cast<std::uint8_t>(missing.source()),
        static_cast<std::uint8_t>(SkyEphemerisAssetStatus::Source::MissingInstalledFallback)
    );
    QVERIFY(!missing.isInstalled());
    QCOMPARE(missing.displayText(), QString("Bundled"));
    QCOMPARE(missing.statusText(), QString("Bundled fallback (installed data missing)"));

    const SkyEphemerisAssetStatus installed =
        SkyEphemerisAssetStatus::fromStatusText(QStringLiteral("Installed: 2026a"));
    QCOMPARE(
        static_cast<std::uint8_t>(installed.source()),
        static_cast<std::uint8_t>(SkyEphemerisAssetStatus::Source::Installed)
    );
    QVERIFY(installed.isInstalled());
    QCOMPARE(installed.version(), QString("2026a"));
    QCOMPARE(installed.displayText(), QString("2026a"));
    QCOMPARE(installed.statusText(), QString("Installed: 2026a"));
}

QTEST_GUILESS_MAIN(SkyEphemerisDataStatusTests)

#include "SkyEphemerisDataStatusTests.moc"
