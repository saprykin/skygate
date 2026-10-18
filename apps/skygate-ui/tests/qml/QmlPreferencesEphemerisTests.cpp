#include "engine/EphemerisDataManifest.hpp"
#include "settings/SkyPersistedStateSnapshot.hpp"
#include "settings/SkySettingsStore.hpp"
#include "support/QmlPreferencesTestSupport.hpp"
#include "time/CalendarTime.hpp"

#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QPointF>
#include <QQuickItem>
#include <QSettings>
#include <QUrl>
#include <QWindow>

#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr std::string_view kEmptySha256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

skygate::ephemeris::EphemerisDateRange testValidityRange()
{
    const auto start = skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::core::CivilDateTime{
            .astronomicalYear = 1900,
            .month = 1,
            .day = 1,
            .timeScale = skygate::core::TimeScale::Utc,
        }
    );
    const auto end = skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::core::CivilDateTime{
            .astronomicalYear = 2100,
            .month = 1,
            .day = 1,
            .timeScale = skygate::core::TimeScale::Utc,
        }
    );
    Q_ASSERT(start.has_value());
    Q_ASSERT(end.has_value());
    return skygate::ephemeris::EphemerisDateRange{
        .id = "test-range",
        .displayName = "Test range",
        .start = *start,
        .end = *end,
    };
}

skygate::ephemeris::EphemerisDataManifest::Asset
emptyKernelAsset(std::string id, std::string profileId, std::string version, std::string relativePath)
{
    skygate::ephemeris::EphemerisDataManifest::Asset asset;
    asset.id = std::move(id);
    asset.kind = skygate::ephemeris::EphemerisDataManifest::AssetKind::SolarSystemKernel;
    asset.profileId = std::move(profileId);
    asset.version = std::move(version);
    asset.relativePath = std::move(relativePath);
    asset.checksum.algorithm = "sha256";
    asset.checksum.value = std::string{kEmptySha256};
    asset.compression.kind = skygate::ephemeris::EphemerisDataManifest::CompressionKind::None;
    asset.compression.uncompressedSizeBytes = 0U;
    asset.validityRange = testValidityRange();
    return asset;
}

skygate::ephemeris::EphemerisDataManifest::Asset emptyDataAsset(
    std::string id,
    const skygate::ephemeris::EphemerisDataManifest::AssetKind kind,
    std::string version,
    std::string relativePath
)
{
    skygate::ephemeris::EphemerisDataManifest::Asset asset =
        emptyKernelAsset(std::move(id), "support-data", std::move(version), std::move(relativePath));
    asset.kind = kind;
    return asset;
}

void writeInstalledEphemerisSettings(
    const QString& kernelPath,
    const QString& earthOrientationPath,
    const QString& leapSecondPath,
    const QString& deltaTPath
)
{
    QSettings settings;
    settings.setValue(
        QStringLiteral("skyContext/ephemerisData/installedKernelAssetId"), QStringLiteral("de441-kernel")
    );
    settings.setValue(
        QStringLiteral("skyContext/ephemerisData/installedKernelProfileId"), QStringLiteral("de441-long-range")
    );
    settings.setValue(QStringLiteral("skyContext/ephemerisData/installedKernelPath"), kernelPath);
    settings.setValue(QStringLiteral("skyContext/ephemerisData/installedKernelVersion"), QStringLiteral("DE441-test"));
    settings.setValue(QStringLiteral("skyContext/ephemerisData/installedEarthOrientationPath"), earthOrientationPath);
    settings.setValue(
        QStringLiteral("skyContext/ephemerisData/installedEarthOrientationVersion"), QStringLiteral("2026-05-07")
    );
    settings.setValue(QStringLiteral("skyContext/ephemerisData/installedLeapSecondTablePath"), leapSecondPath);
    settings.setValue(
        QStringLiteral("skyContext/ephemerisData/installedLeapSecondTableVersion"), QStringLiteral("2026a")
    );
    settings.setValue(QStringLiteral("skyContext/ephemerisData/installedDeltaTDataPath"), deltaTPath);
    settings.setValue(QStringLiteral("skyContext/ephemerisData/installedDeltaTDataVersion"), QStringLiteral("2026a"));
    settings.setValue(QStringLiteral("skyContext/ephemerisData/dataRevisionToken"), QStringLiteral("de441-test"));
    settings.setValue(QStringLiteral("skyContext/ephemerisData/lastUpdateResult"), QStringLiteral("Installed DE441"));
}

skygate::ephemeris::EphemerisDataManifest minimalUpdateManifest()
{
    skygate::ephemeris::EphemerisDataManifest manifest;
    manifest.profiles.push_back(
        skygate::ephemeris::EphemerisDataManifest::Profile{
            .id = "de440s-short-range",
            .displayName = "DE440sShortRange",
            .bundled = true,
            .longRange = false,
            .assetIds = {"de440s-kernel"},
        }
    );
    manifest.profiles.push_back(
        skygate::ephemeris::EphemerisDataManifest::Profile{
            .id = "de441-long-range",
            .displayName = "DE441 long range",
            .bundled = false,
            .longRange = true,
            .assetIds = {"de441-kernel"},
        }
    );
    manifest.profiles.push_back(
        skygate::ephemeris::EphemerisDataManifest::Profile{
            .id = "support-data",
            .displayName = "Time and Earth data",
            .bundled = false,
            .longRange = false,
            .assetIds = {"support-earth-orientation", "support-leap-seconds", "support-delta-t"},
        }
    );
    manifest.assets.push_back(
        emptyKernelAsset("de440s-kernel", "de440s-short-range", "DE440s-test", "de440s-short-range/kernels/de440s.bsp")
    );
    manifest.assets.push_back(
        emptyKernelAsset("de441-kernel", "de441-long-range", "DE441-test", "de441/kernels/de441.bsp")
    );
    manifest.assets.push_back(emptyDataAsset(
        "support-earth-orientation",
        skygate::ephemeris::EphemerisDataManifest::AssetKind::EarthOrientationData,
        "2026-05-07",
        "time/eop.txt"
    ));
    manifest.assets.push_back(emptyDataAsset(
        "support-leap-seconds",
        skygate::ephemeris::EphemerisDataManifest::AssetKind::LeapSecondTable,
        "2026a",
        "time/leap-seconds.list"
    ));
    manifest.assets.push_back(emptyDataAsset(
        "support-delta-t",
        skygate::ephemeris::EphemerisDataManifest::AssetKind::DeltaTData,
        "2026a",
        "time/delta-t.data"
    ));
    return manifest;
}

skygate::ephemeris::EphemerisDataManifest supportDataUpdateManifest()
{
    skygate::ephemeris::EphemerisDataManifest manifest = minimalUpdateManifest();
    for (skygate::ephemeris::EphemerisDataManifest::Asset& asset : manifest.assets) {
        if (asset.id == "support-earth-orientation") {
            asset.version = "2026-06-01";
        } else if (asset.id == "support-leap-seconds" || asset.id == "support-delta-t") {
            asset.version = "2026b";
        }
    }
    return manifest;
}

bool writeStagedEphemerisAssets(const QString& root, const skygate::ephemeris::EphemerisDataManifest& manifest)
{
    for (const skygate::ephemeris::EphemerisDataManifest::Asset& asset : manifest.assets) {
        if (!writeFile(root + QLatin1Char('/') + QString::fromStdString(asset.relativePath), QByteArray{})) {
            return false;
        }
    }
    return true;
}

QString manifestAssetVersion(const skygate::ephemeris::EphemerisDataManifest& manifest, const std::string& assetId)
{
    const skygate::ephemeris::EphemerisDataManifest::Asset* asset = manifest.asset(assetId);
    return asset != nullptr ? QString::fromStdString(asset->version).trimmed() : QString();
}

QString preferencesUpdateManifestJson(
    const QString& updateResourceRoot,
    const QString& earthOrientationVersion,
    const QString& leapSecondVersion,
    const QString& deltaTVersion
)
{
    const auto sourceUrl = [&updateResourceRoot](const QString& relativePath) {
        return QUrl::fromLocalFile(updateResourceRoot + QLatin1Char('/') + relativePath).toString();
    };
    const QString emptySha256 = QString::fromLatin1(kEmptySha256.data(), static_cast<qsizetype>(kEmptySha256.size()));

    return QStringLiteral(R"({
  "schemaVersion": 1,
  "id": "test-preferences-data",
  "displayName": "Test preferences data",
  "version": "2026a",
  "provenance": "preferences test",
  "dateRanges": [
    {
      "id": "test-range",
      "displayName": "Test range",
      "start": "1900-01-01",
      "end": "2100-01-01"
    }
  ],
  "profiles": [
    {
      "id": "de440s-short-range",
      "displayName": "DE440sShortRange",
      "bundled": true,
      "longRange": false,
      "assetIds": ["de440s-kernel"]
    },
    {
      "id": "de441-long-range",
      "displayName": "DE441 long range",
      "bundled": false,
      "longRange": true,
      "assetIds": ["de441-kernel"]
    },
    {
      "id": "support-data",
      "displayName": "Time and Earth data",
      "bundled": false,
      "longRange": false,
      "assetIds": ["support-earth-orientation", "support-leap-seconds", "support-delta-t"]
    }
  ],
  "assets": [
    {
      "id": "de440s-kernel",
      "kind": "solar-system-kernel",
      "profileId": "de440s-short-range",
      "version": "DE440s-test",
      "sourceUrl": "%1",
      "relativePath": "de440s-short-range/kernels/de440s.bsp",
      "checksum": { "algorithm": "sha256", "value": "%6" },
      "compression": { "format": "none", "uncompressedSizeBytes": 0 },
      "validityRange": {
        "id": "kernel-range",
        "displayName": "Kernel range",
        "start": "1900-01-01",
        "end": "2100-01-01"
      }
    },
    {
      "id": "de441-kernel",
      "kind": "solar-system-kernel",
      "profileId": "de441-long-range",
      "version": "DE441-test",
      "sourceUrl": "%2",
      "relativePath": "de441/kernels/de441.bsp",
      "checksum": { "algorithm": "sha256", "value": "%6" },
      "compression": { "format": "none", "uncompressedSizeBytes": 0 },
      "validityRange": {
        "id": "kernel-range",
        "displayName": "Kernel range",
        "start": "1900-01-01",
        "end": "2100-01-01"
      }
    },
    {
      "id": "support-earth-orientation",
      "kind": "earth-orientation-data",
      "profileId": "support-data",
      "version": "%7",
      "sourceUrl": "%3",
      "relativePath": "time/eop.txt",
      "checksum": { "algorithm": "sha256", "value": "%6" },
      "compression": { "format": "none", "uncompressedSizeBytes": 0 },
      "validityRange": {
        "id": "eop-range",
        "displayName": "Earth orientation range",
        "start": "1900-01-01",
        "end": "2100-01-01"
      }
    },
    {
      "id": "support-leap-seconds",
      "kind": "leap-second-table",
      "profileId": "support-data",
      "version": "%8",
      "sourceUrl": "%4",
      "relativePath": "time/leap-seconds.list",
      "checksum": { "algorithm": "sha256", "value": "%6" },
      "compression": { "format": "none", "uncompressedSizeBytes": 0 },
      "validityRange": {
        "id": "leap-second-range",
        "displayName": "Leap-second table range",
        "start": "1972-01-01",
        "end": "2027-06-28"
      }
    },
    {
      "id": "support-delta-t",
      "kind": "delta-t-data",
      "profileId": "support-data",
      "version": "%9",
      "sourceUrl": "%5",
      "relativePath": "time/delta-t.data",
      "checksum": { "algorithm": "sha256", "value": "%6" },
      "compression": { "format": "none", "uncompressedSizeBytes": 0 },
      "validityRange": {
        "id": "delta-t-range",
        "displayName": "Delta T range",
        "start": "-13200-01-01",
        "end": "13200-12-31"
      }
    }
  ]
})")
        .arg(
            sourceUrl(QStringLiteral("de440s-short-range/kernels/de440s.bsp")),
            sourceUrl(QStringLiteral("de441/kernels/de441.bsp")),
            sourceUrl(QStringLiteral("time/eop.txt")),
            sourceUrl(QStringLiteral("time/leap-seconds.list")),
            sourceUrl(QStringLiteral("time/delta-t.data")),
            emptySha256,
            earthOrientationVersion,
            leapSecondVersion,
            deltaTVersion
        );
}

QQuickItem* comboPopupDelegate(QObject* combo, const int index)
{
    QObject* popup = qvariant_cast<QObject*>(combo->property("popup"));
    if (popup == nullptr) {
        return nullptr;
    }
    QObject* listView = qvariant_cast<QObject*>(popup->property("contentItem"));
    if (listView == nullptr) {
        return nullptr;
    }
    QQuickItem* delegate = nullptr;
    if (!QMetaObject::invokeMethod(listView, "itemAtIndex", Q_RETURN_ARG(QQuickItem*, delegate), Q_ARG(int, index))) {
        return nullptr;
    }
    return delegate;
}

void clickComboPopupDelegate(QWindow* window, QObject* combo, const int index)
{
    QVERIFY(combo != nullptr);
    QObject* popup = qvariant_cast<QObject*>(combo->property("popup"));
    QVERIFY(popup != nullptr);
    QVERIFY(QMetaObject::invokeMethod(popup, "open"));
    QTRY_VERIFY(popup->property("opened").toBool());

    QQuickItem* delegate = comboPopupDelegate(combo, index);
    QVERIFY(delegate != nullptr);
    const QPoint clickPoint =
        delegate->mapToScene(QPointF(delegate->width() * 0.5, delegate->height() * 0.5)).toPoint();
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, clickPoint);
    QCoreApplication::processEvents();
}

std::unique_ptr<SkyContextController> makeControllerWithManifest(
    const skygate::ephemeris::EphemerisDataManifest& manifest,
    const QString& updateResourceRoot,
    const QString& writableCacheRoot
)
{
    const QString manifestPath = updateResourceRoot + QStringLiteral("/update-manifest.json");
    if (!writeFile(
            manifestPath,
            preferencesUpdateManifestJson(
                updateResourceRoot,
                manifestAssetVersion(manifest, "support-earth-orientation"),
                manifestAssetVersion(manifest, "support-leap-seconds"),
                manifestAssetVersion(manifest, "support-delta-t")
            )
                .toUtf8()
        )) {
        return {};
    }

    SkyPersistedStateSnapshot savedSettings;
    savedSettings.ephemerisSettingsPresent = true;
    savedSettings.ephemeris.updateManifestUrl = QUrl::fromLocalFile(manifestPath).toString();
    SkySettingsStore settingsStore;
    if (!settingsStore.saveState(savedSettings)) {
        return {};
    }

    auto starCatalog = skygate::ephemeris::CatalogFactory::createBundledStarCatalog();
    if (starCatalog == nullptr) {
        return {};
    }
    auto ephemerisEngineResult = skygate::ephemeris::EphemerisEngineFactory::create(*starCatalog);
    if (!ephemerisEngineResult.isSuccess() || ephemerisEngineResult.engine == nullptr) {
        return {};
    }
    auto ephemerisEngine = std::move(ephemerisEngineResult.engine);

    SkyContextController::InitializationOptions options;
    options.ephemerisFactoryInputs.dataManifest = &manifest;
    options.ephemerisFactoryInputs.updateResourceRoot = updateResourceRoot;
    options.ephemerisFactoryInputs.writableCacheRoot = writableCacheRoot;
    return std::make_unique<SkyContextController>(std::move(starCatalog), std::move(ephemerisEngine), options, nullptr);
}

}  // namespace

class QmlPreferencesEphemerisTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void ephemerisEngineControlsBindDraftAndVisibility();
    void ephemerisDataControlsShowFallbackAndUpdateMode();
    void ephemerisControlsShowInstalledStateAndClearCache();
    void downloadComboPopupClicksInvokeControllerActions();

private:
    QmlSettingsFixture m_settings;
};

void QmlPreferencesEphemerisTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("QmlPreferencesEphemerisTests")));
}

void QmlPreferencesEphemerisTests::init()
{
    m_settings.resetForCurrentTest();
    QSettings settings;
    settings.setValue(
        QStringLiteral("skyContext/catalogCollectionCachePath"),
        m_settings.cachePath(QStringLiteral("catalog-collection-cache"))
    );
    QDir(m_settings.cachePath(QStringLiteral("catalog-collection-cache"))).removeRecursively();
}

void QmlPreferencesEphemerisTests::ephemerisEngineControlsBindDraftAndVisibility()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createInlineComponent(
        engine,
        QStringLiteral(R"(
        import QtQuick
        Item {
            id: root
            width: 900
            height: 620
            property alias draft: draft
            PreferencesDraft {
                id: draft
                skyContextController: skyContext
                Component.onCompleted: resetFromContext()
            }
            PreferencesEngineSection {
                anchors.fill: parent
                skyContextController: skyContext
                preferencesDraft: draft
            }
        }
    )"),
        QStringLiteral("PreferencesEphemerisEngineSectionTest.qml")
    );
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    QObject* draft = qvariant_cast<QObject*>(root->property("draft"));
    QVERIFY(draft != nullptr);

    QObject* engineCombo = firstObjectWithObjectName(root, QStringLiteral("ephemerisEngineSelectorCombo"));
    QVERIFY(engineCombo != nullptr);
    QCOMPARE(engineCombo->property("count").toInt(), 2);
    QCOMPARE(engineCombo->property("currentIndex").toInt(), 0);

    QObject* correctionCombo = firstObjectWithObjectName(root, QStringLiteral("ephemerisCorrectionPresetCombo"));
    QVERIFY(correctionCombo != nullptr);
    QVERIFY(!correctionCombo->property("visible").toBool());

    QObject* refractionCheckBox = firstObjectWithObjectName(root, QStringLiteral("ephemerisRefractionCheckBox"));
    QVERIFY(refractionCheckBox != nullptr);
    QVERIFY(!refractionCheckBox->property("visible").toBool());

    draft->setProperty("ephemerisEngineKindIndex", 1);
    draft->setProperty("ephemerisCorrectionPresetIndex", 3);
    draft->setProperty("ephemerisRefractionEnabled", true);
    QCoreApplication::processEvents();
    QTRY_COMPARE(engineCombo->property("currentIndex").toInt(), 1);
    QTRY_VERIFY(correctionCombo->property("visible").toBool());
    QCOMPARE(correctionCombo->property("count").toInt(), 4);
    QCOMPARE(correctionCombo->property("currentIndex").toInt(), 3);
    QVERIFY(refractionCheckBox->property("visible").toBool());
    QVERIFY(refractionCheckBox->property("checked").toBool());

    draft->setProperty("ephemerisCorrectionPresetIndex", 1);
    draft->setProperty("ephemerisRefractionEnabled", false);
    QCoreApplication::processEvents();
    QTRY_COMPARE(correctionCombo->property("currentIndex").toInt(), 1);
    QTRY_VERIFY(!refractionCheckBox->property("checked").toBool());

    auto* pressureInput = firstQuickItemWithObjectName(root, QStringLiteral("ephemerisPressureInput"));
    QVERIFY(pressureInput != nullptr);
    QVERIFY(!pressureInput->isVisible());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesEphemerisTests::ephemerisDataControlsShowFallbackAndUpdateMode()
{
    const skygate::ephemeris::EphemerisDataManifest manifest = minimalUpdateManifest();
    const QString updateResourceRoot = m_settings.cachePath(QStringLiteral("ephemeris-source"));
    QVERIFY(writeStagedEphemerisAssets(updateResourceRoot, manifest));
    auto controller = makeControllerWithManifest(
        manifest, updateResourceRoot, m_settings.cachePath(QStringLiteral("ephemeris-cache"))
    );
    QVERIFY(controller != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createInlineComponent(
        engine,
        QStringLiteral(R"(
        import QtQuick
        Item {
            id: root
            width: 900
            height: 720
            property alias draft: draft
            PreferencesDraft {
                id: draft
                skyContextController: skyContext
                Component.onCompleted: resetFromContext()
            }
            PreferencesEngineSection {
                anchors.fill: parent
                skyContextController: skyContext
                preferencesDraft: draft
            }
        }
    )"),
        QStringLiteral("PreferencesEphemerisDataFallbackTest.qml")
    );
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    QObject* modernStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisShortRangeKernelStatusLabel"));
    QObject* eopStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisEarthOrientationStatusLabel"));
    QObject* leapSecondStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisLeapSecondStatusLabel"));
    QObject* deltaTStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisDeltaTStatusLabel"));
    QObject* kernelDownloadCombo = firstObjectWithObjectName(root, QStringLiteral("ephemerisKernelDownloadCombo"));
    QObject* supportDownloadCombo =
        firstObjectWithObjectName(root, QStringLiteral("ephemerisSupportDataDownloadCombo"));
    QVERIFY(modernStatus != nullptr);
    QVERIFY(eopStatus != nullptr);
    QVERIFY(leapSecondStatus != nullptr);
    QVERIFY(deltaTStatus != nullptr);
    QVERIFY(kernelDownloadCombo != nullptr);
    QVERIFY(supportDownloadCombo != nullptr);

    QCOMPARE(controller->ephemerisKernelDisplayText(), QString("Bundled"));
    QCOMPARE(modernStatus->property("text").toString(), QString("Bundled"));
    QCOMPARE(modernStatus->property("text").toString(), controller->ephemerisKernelDisplayText());
    QCOMPARE(eopStatus->property("text").toString(), QString("Bundled"));
    QCOMPARE(leapSecondStatus->property("text").toString(), QString("Bundled"));
    QCOMPARE(deltaTStatus->property("text").toString(), QString("Bundled"));
    QCOMPARE(kernelDownloadCombo->property("displayText").toString(), QString("Select kernel..."));
    QCOMPARE(supportDownloadCombo->property("displayText").toString(), QString("Select data..."));
    QVERIFY(kernelDownloadCombo->property("enabled").toBool());
    QVERIFY(supportDownloadCombo->property("enabled").toBool());

    QVERIFY(QMetaObject::invokeMethod(kernelDownloadCombo, "activated", Q_ARG(int, 2)));
    QTRY_COMPARE(modernStatus->property("text").toString(), QString("DE441"));
    QCOMPARE(modernStatus->property("text").toString(), controller->ephemerisKernelDisplayText());
    QTRY_COMPARE(eopStatus->property("text").toString(), QString("Bundled"));
    QTRY_COMPARE(leapSecondStatus->property("text").toString(), QString("Bundled"));
    QTRY_COMPARE(deltaTStatus->property("text").toString(), QString("Bundled"));
    QCOMPARE(controller->ephemerisDataStatusText(), QString("Ephemeris data: Installed data active"));

    QVERIFY(QMetaObject::invokeMethod(kernelDownloadCombo, "activated", Q_ARG(int, 1)));
    QTRY_COMPARE(modernStatus->property("text").toString(), QString("DE440s"));
    QCOMPARE(modernStatus->property("text").toString(), controller->ephemerisKernelDisplayText());

    controller->setEphemerisDataOnlineUpdatesEnabled(false);
    QTRY_VERIFY(supportDownloadCombo->property("enabled").toBool());
    QTRY_VERIFY(kernelDownloadCombo->property("enabled").toBool());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesEphemerisTests::ephemerisControlsShowInstalledStateAndClearCache()
{
    const QString kernelPath = m_settings.cachePath(QStringLiteral("de441.bsp"));
    const QString earthOrientationPath = m_settings.cachePath(QStringLiteral("eop.csv"));
    const QString leapSecondPath = m_settings.cachePath(QStringLiteral("leap-seconds.list"));
    const QString deltaTPath = m_settings.cachePath(QStringLiteral("delta-t.csv"));
    QVERIFY(writeFile(kernelPath, QByteArray(2 * 1024 * 1024, 'k')));
    QVERIFY(writeFile(earthOrientationPath, QByteArray(1024 * 1024, 'e')));
    QVERIFY(writeFile(leapSecondPath, QByteArray(1024 * 1024, 'l')));
    QVERIFY(writeFile(deltaTPath, QByteArray(1024 * 1024, 'd')));
    writeInstalledEphemerisSettings(kernelPath, earthOrientationPath, leapSecondPath, deltaTPath);

    const skygate::ephemeris::EphemerisDataManifest updateManifest = supportDataUpdateManifest();
    auto controller = makeControllerWithManifest(
        updateManifest,
        m_settings.cachePath(QStringLiteral("ephemeris-source")),
        m_settings.cachePath(QStringLiteral("ephemeris-cache"))
    );
    QVERIFY(controller != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createInlineComponent(
        engine,
        QStringLiteral(R"(
        import QtQuick
        Item {
            id: root
            width: 900
            height: 720
            property alias draft: draft
            PreferencesDraft {
                id: draft
                skyContextController: skyContext
                Component.onCompleted: resetFromContext()
            }
            PreferencesEngineSection {
                anchors.fill: parent
                skyContextController: skyContext
                preferencesDraft: draft
            }
        }
    )"),
        QStringLiteral("PreferencesEphemerisDataInstalledTest.qml")
    );
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    QObject* modernStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisShortRangeKernelStatusLabel"));
    QObject* eopStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisEarthOrientationStatusLabel"));
    QObject* leapSecondStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisLeapSecondStatusLabel"));
    QObject* deltaTStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisDeltaTStatusLabel"));
    QObject* kernelCacheSize =
        firstObjectWithObjectName(root, QStringLiteral("ephemerisPlanetaryKernelCacheSizeLabel"));
    QObject* kernelClearButton =
        firstObjectWithObjectName(root, QStringLiteral("ephemerisPlanetaryKernelClearCacheButton"));
    QObject* supportCacheSize = firstObjectWithObjectName(root, QStringLiteral("ephemerisSupportDataCacheSizeLabel"));
    QObject* supportDownloadCombo =
        firstObjectWithObjectName(root, QStringLiteral("ephemerisSupportDataDownloadCombo"));
    QObject* supportClearButton =
        firstObjectWithObjectName(root, QStringLiteral("ephemerisSupportDataClearCacheButton"));
    QVERIFY(modernStatus != nullptr);
    QVERIFY(eopStatus != nullptr);
    QVERIFY(leapSecondStatus != nullptr);
    QVERIFY(deltaTStatus != nullptr);
    QVERIFY(kernelCacheSize != nullptr);
    QVERIFY(kernelClearButton != nullptr);
    QVERIFY(supportCacheSize != nullptr);
    QVERIFY(supportDownloadCombo != nullptr);
    QVERIFY(supportClearButton != nullptr);

    QCOMPARE(modernStatus->property("text").toString(), QString("DE441"));
    QCOMPARE(modernStatus->property("text").toString(), controller->ephemerisKernelDisplayText());
    QCOMPARE(kernelCacheSize->property("text").toString(), QString("2.0 MB"));
    QCOMPARE(supportCacheSize->property("text").toString(), QString("3.0 MB"));
    QCOMPARE(eopStatus->property("text").toString(), QString("2026-05-07"));
    QCOMPARE(leapSecondStatus->property("text").toString(), QString("2026a"));
    QCOMPARE(deltaTStatus->property("text").toString(), QString("2026a"));
    QVERIFY(supportDownloadCombo->property("enabled").toBool());

    QVERIFY(QMetaObject::invokeMethod(supportDownloadCombo, "activated", Q_ARG(int, 2)));
    QTRY_COMPARE(eopStatus->property("text").toString(), QString("2026-05-07 (available: 2026-06-01)"));
    QTRY_COMPARE(leapSecondStatus->property("text").toString(), QString("2026a (available: 2026b)"));
    QTRY_COMPARE(deltaTStatus->property("text").toString(), QString("2026a (available: 2026b)"));

    QVERIFY(activateControl(kernelClearButton));
    QTRY_COMPARE(modernStatus->property("text").toString(), QString("Bundled"));
    QCOMPARE(modernStatus->property("text").toString(), controller->ephemerisKernelDisplayText());
    QTRY_COMPARE(kernelCacheSize->property("text").toString(), QString("0 MB"));
    QCOMPARE(eopStatus->property("text").toString(), QString("2026-05-07 (available: 2026-06-01)"));
    QVERIFY(!QFileInfo::exists(kernelPath));
    QVERIFY(QFileInfo::exists(earthOrientationPath));

    QVERIFY(activateControl(supportClearButton));
    QTRY_COMPARE(modernStatus->property("text").toString(), QString("Bundled"));
    QTRY_COMPARE(supportCacheSize->property("text").toString(), QString("0 MB"));
    QCOMPARE(controller->ephemerisDataStatusText(), QString("Ephemeris data: Bundled fallback"));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesEphemerisTests::downloadComboPopupClicksInvokeControllerActions()
{
    const skygate::ephemeris::EphemerisDataManifest manifest = supportDataUpdateManifest();
    const QString updateResourceRoot = m_settings.cachePath(QStringLiteral("ephemeris-source"));
    QVERIFY(writeStagedEphemerisAssets(updateResourceRoot, manifest));
    auto controller = makeControllerWithManifest(
        manifest, updateResourceRoot, m_settings.cachePath(QStringLiteral("ephemeris-cache"))
    );
    QVERIFY(controller != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createInlineComponent(
        engine,
        QStringLiteral(R"(
        import QtQuick
        Item {
            id: root
            width: 900
            height: 720
            property alias draft: draft
            PreferencesDraft {
                id: draft
                skyContextController: skyContext
                Component.onCompleted: resetFromContext()
            }
            PreferencesEngineSection {
                anchors.fill: parent
                skyContextController: skyContext
                preferencesDraft: draft
            }
        }
    )"),
        QStringLiteral("PreferencesEphemerisPopupActivationTest.qml")
    );
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    QObject* kernelDownloadCombo = firstObjectWithObjectName(root, QStringLiteral("ephemerisKernelDownloadCombo"));
    QObject* supportDownloadCombo =
        firstObjectWithObjectName(root, QStringLiteral("ephemerisSupportDataDownloadCombo"));
    QVERIFY(kernelDownloadCombo != nullptr);
    QVERIFY(supportDownloadCombo != nullptr);

    ExposedQuickWindow exposed(root);
    QWindow* window = exposed.window();

    clickComboPopupDelegate(window, supportDownloadCombo, 2);
    QTRY_COMPARE(
        controller->ephemerisDataStatusText(), QString("Ephemeris data: Updates available for time and Earth data")
    );

    clickComboPopupDelegate(window, kernelDownloadCombo, 2);
    QTRY_COMPARE(controller->ephemerisKernelDisplayText(), QString("DE441"));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

SKYGATE_QML_TEST_MAIN(QmlPreferencesEphemerisTests)

#include "QmlPreferencesEphemerisTests.moc"
