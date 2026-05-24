#include "time/CalendarTime.hpp"
#include "QmlPreferencesTestSupport.hpp"

#include "engine/highprecision/EphemerisDataManifest.hpp"

#include <QFileInfo>
#include <QSettings>

#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr std::string_view kEmptySha256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

skygate::ephemeris::EphemerisDateRange testValidityRange()
{
    const auto start = skygate::ephemeris::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::ephemeris::CivilDateTime{
            .astronomicalYear = 1900,
            .month = 1,
            .day = 1,
            .timeScale = skygate::ephemeris::TimeScale::Utc,
        }
    );
    const auto end = skygate::ephemeris::CalendarTime::astronomicalEpochFromCivilDateTime(
        skygate::ephemeris::CivilDateTime{
            .astronomicalYear = 2100,
            .month = 1,
            .day = 1,
            .timeScale = skygate::ephemeris::TimeScale::Utc,
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

skygate::ephemeris::EphemerisDataManifestAsset
emptyKernelAsset(std::string id, std::string profileId, std::string version, std::string relativePath)
{
    skygate::ephemeris::EphemerisDataManifestAsset asset;
    asset.id = std::move(id);
    asset.kind = skygate::ephemeris::EphemerisDataManifestAssetKind::SolarSystemKernel;
    asset.profileId = std::move(profileId);
    asset.version = std::move(version);
    asset.relativePath = std::move(relativePath);
    asset.checksum.algorithm = "sha256";
    asset.checksum.value = std::string{kEmptySha256};
    asset.compression.kind = skygate::ephemeris::EphemerisDataManifestCompressionKind::None;
    asset.compression.uncompressedSizeBytes = 0U;
    asset.validityRange = testValidityRange();
    return asset;
}

skygate::ephemeris::EphemerisDataManifestAsset emptyDataAsset(
    std::string id,
    const skygate::ephemeris::EphemerisDataManifestAssetKind kind,
    std::string version,
    std::string relativePath
)
{
    skygate::ephemeris::EphemerisDataManifestAsset asset =
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
        skygate::ephemeris::EphemerisDataManifestProfile{
            .id = "de440s-short-range",
            .displayName = "DE440sShortRange",
            .bundled = true,
            .longRange = false,
            .assetIds = {"de440s-kernel"},
        }
    );
    manifest.profiles.push_back(
        skygate::ephemeris::EphemerisDataManifestProfile{
            .id = "de441-long-range",
            .displayName = "DE441 long range",
            .bundled = false,
            .longRange = true,
            .assetIds = {"de441-kernel"},
        }
    );
    manifest.profiles.push_back(
        skygate::ephemeris::EphemerisDataManifestProfile{
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
        skygate::ephemeris::EphemerisDataManifestAssetKind::EarthOrientationData,
        "2026-05-07",
        "time/eop.txt"
    ));
    manifest.assets.push_back(emptyDataAsset(
        "support-leap-seconds",
        skygate::ephemeris::EphemerisDataManifestAssetKind::LeapSecondTable,
        "2026a",
        "time/leap-seconds.list"
    ));
    manifest.assets.push_back(emptyDataAsset(
        "support-delta-t", skygate::ephemeris::EphemerisDataManifestAssetKind::DeltaTData, "2026a", "time/delta-t.data"
    ));
    return manifest;
}

skygate::ephemeris::EphemerisDataManifest supportDataUpdateManifest()
{
    skygate::ephemeris::EphemerisDataManifest manifest = minimalUpdateManifest();
    for (skygate::ephemeris::EphemerisDataManifestAsset& asset : manifest.assets) {
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
    for (const skygate::ephemeris::EphemerisDataManifestAsset& asset : manifest.assets) {
        if (!writeFile(root + QLatin1Char('/') + QString::fromStdString(asset.relativePath), QByteArray{})) {
            return false;
        }
    }
    return true;
}

std::unique_ptr<SkyContextController> makeControllerWithManifest(
    const skygate::ephemeris::EphemerisDataManifest& manifest,
    const QString& updateResourceRoot,
    const QString& writableCacheRoot
)
{
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

class QmlPreferencesCatalogTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void catalogSectionBindsDraftAndControls();
    void ephemerisEngineControlsBindDraftAndVisibility();
    void ephemerisDataControlsShowFallbackAndUpdateMode();
    void ephemerisDataControlsShowInstalledStateAndClearCache();
    void catalogSectionDownloadsAppliesClearsAndRestoresCatalogs();

private:
    QmlSettingsFixture m_settings;
};

void QmlPreferencesCatalogTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("QmlPreferencesCatalogTests")));
}

void QmlPreferencesCatalogTests::init()
{
    m_settings.resetForCurrentTest();
}

void QmlPreferencesCatalogTests::catalogSectionBindsDraftAndControls()
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
            height: 520
            property alias draft: draft
            PreferencesDraft {
                id: draft
                skyContextController: skyContext
                Component.onCompleted: resetFromContext()
            }
            PreferencesCatalogSection {
                anchors.fill: parent
                skyContextController: skyContext
                preferencesDraft: draft
            }
        }
    )"),
        QStringLiteral("PreferencesCatalogSectionBehaviorTest.qml")
    );
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    ExposedQuickWindow exposed(root);
    QObject* draft = qvariant_cast<QObject*>(root->property("draft"));
    QVERIFY(draft != nullptr);

    QObject* catalogCombo = firstObjectWithObjectName(root, QStringLiteral("starCatalogPresetCombo"));
    QVERIFY(catalogCombo != nullptr);
    QCOMPARE(catalogCombo->property("currentIndex").toInt(), 0);

    QObject* useButton = firstObjectWithObjectName(root, QStringLiteral("starCatalogUseButton"));
    QVERIFY(useButton != nullptr);
    QVERIFY(useButton->property("enabled").toBool());

    draft->setProperty("catalogPresetIndex", 2);
    QCoreApplication::processEvents();
    QTRY_COMPARE(catalogCombo->property("currentIndex").toInt(), 2);
    QVERIFY(!useButton->property("enabled").toBool());

    auto* catalogUrlInput = firstQuickItemWithObjectName(root, QStringLiteral("starCatalogUrlInput"));
    QVERIFY(catalogUrlInput != nullptr);
    QTRY_VERIFY(catalogUrlInput->isVisible());

    QVERIFY(QMetaObject::invokeMethod(catalogUrlInput, "forceActiveFocus"));
    QVERIFY(QMetaObject::invokeMethod(catalogUrlInput, "selectAll"));
    constexpr const char* kCatalogUrl = "https://example.test/custom-stars.csv";
    commitText(exposed.window(), QString::fromUtf8(kCatalogUrl));
    QTRY_COMPARE(draft->property("catalogUrlText").toString(), QString(kCatalogUrl));

    QObject* downloadButton = firstObjectWithObjectName(root, QStringLiteral("starCatalogDownloadButton"));
    QVERIFY(downloadButton != nullptr);
    QVERIFY(downloadButton->property("enabled").toBool());
    QVERIFY(firstObjectWithObjectName(root, QStringLiteral("ephemerisEngineSelectorCombo")) == nullptr);
    QVERIFY(firstObjectWithObjectName(root, QStringLiteral("ephemerisDataUpdateButton")) == nullptr);
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesCatalogTests::ephemerisEngineControlsBindDraftAndVisibility()
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

void QmlPreferencesCatalogTests::ephemerisDataControlsShowFallbackAndUpdateMode()
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

    QCOMPARE(modernStatus->property("text").toString(), QString("Bundled"));
    QCOMPARE(eopStatus->property("text").toString(), QString("Bundled"));
    QCOMPARE(leapSecondStatus->property("text").toString(), QString("Bundled"));
    QCOMPARE(deltaTStatus->property("text").toString(), QString("Bundled"));
    QCOMPARE(kernelDownloadCombo->property("displayText").toString(), QString("Select kernel..."));
    QCOMPARE(supportDownloadCombo->property("displayText").toString(), QString("Select data..."));
    QVERIFY(kernelDownloadCombo->property("enabled").toBool());
    QVERIFY(supportDownloadCombo->property("enabled").toBool());

    QVERIFY(QMetaObject::invokeMethod(kernelDownloadCombo, "activated", Q_ARG(int, 2)));
    QTRY_COMPARE(modernStatus->property("text").toString(), QString("DE441"));
    QTRY_COMPARE(eopStatus->property("text").toString(), QString("Bundled"));
    QTRY_COMPARE(leapSecondStatus->property("text").toString(), QString("Bundled"));
    QTRY_COMPARE(deltaTStatus->property("text").toString(), QString("Bundled"));
    QCOMPARE(controller->ephemerisDataStatusText(), QString("Ephemeris data: Installed data active"));

    controller->setEphemerisDataOnlineUpdatesEnabled(false);
    QTRY_VERIFY(supportDownloadCombo->property("enabled").toBool());
    QTRY_VERIFY(kernelDownloadCombo->property("enabled").toBool());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesCatalogTests::ephemerisDataControlsShowInstalledStateAndClearCache()
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

void QmlPreferencesCatalogTests::catalogSectionDownloadsAppliesClearsAndRestoresCatalogs()
{
    const QString starCatalogPath = m_settings.cachePath(QStringLiteral("download-stars.csv"));
    const QString deepSkyCatalogPath = m_settings.cachePath(QStringLiteral("download-dso.csv"));
    const QByteArray starCatalogPayload =
        sampleHygCsvPayload({1, 900001, "Downloaded Star", "6.7525", "-16.7161", "1.0"});
    const QByteArray deepSkyCatalogPayload = sampleOpenNgcCsvPayload(
        {"NGC0999", "G", "00:42:44.35", "+41:16:08.6", "And", "", "0999", "PGC 9999", "Custom Galaxy"}
    );
    QVERIFY(writeFile(starCatalogPath, starCatalogPayload));
    QVERIFY(writeFile(deepSkyCatalogPath, deepSkyCatalogPayload));
    const QString starCatalogUrl = QUrl::fromLocalFile(starCatalogPath).toString();
    const QString deepSkyCatalogUrl = QUrl::fromLocalFile(deepSkyCatalogPath).toString();

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
            height: 520
            property alias draft: draft
            PreferencesDraft {
                id: draft
                skyContextController: skyContext
                Component.onCompleted: resetFromContext()
            }
            PreferencesCatalogSection {
                anchors.fill: parent
                skyContextController: skyContext
                preferencesDraft: draft
            }
        }
    )"),
        QStringLiteral("PreferencesCatalogDownloadTest.qml")
    );
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    ExposedQuickWindow exposed(root);
    (void)exposed;
    QObject* draft = qvariant_cast<QObject*>(root->property("draft"));
    QVERIFY(draft != nullptr);

    draft->setProperty("catalogPresetIndex", 2);
    draft->setProperty("catalogUrlText", starCatalogUrl);
    QCoreApplication::processEvents();
    QObject* starDownloadButton = firstObjectWithObjectName(root, QStringLiteral("starCatalogDownloadButton"));
    QVERIFY(starDownloadButton != nullptr);
    QVERIFY(activateControl(starDownloadButton));
    QTRY_VERIFY(!controller->downloadingCatalog() && !controller->catalogProcessing());
    QTRY_VERIFY(catalogContainsDisplayName(controller->catalogBodies(), QStringLiteral("Downloaded Star")));
    QVERIFY(QFileInfo::exists(m_settings.cachePath(QStringLiteral("star-cache.csv"))));

    draft->setProperty("deepSkyCatalogPresetIndex", 2);
    draft->setProperty("deepSkyCatalogUrlText", deepSkyCatalogUrl);
    QCoreApplication::processEvents();
    QObject* deepSkyDownloadButton = firstObjectWithObjectName(root, QStringLiteral("deepSkyCatalogDownloadButton"));
    QVERIFY(deepSkyDownloadButton != nullptr);
    QVERIFY(activateControl(deepSkyDownloadButton));
    QTRY_VERIFY(!controller->downloadingCatalog() && !controller->catalogProcessing());
    QTRY_VERIFY(catalogContainsAlias(controller->catalogBodies(), QStringLiteral("Custom Galaxy")));
    QVERIFY(QFileInfo::exists(m_settings.cachePath(QStringLiteral("deep-sky-cache.csv"))));

    QVERIFY(controller->saveSettings());
    auto restoredController = makeController();
    QVERIFY(restoredController != nullptr);
    QTRY_VERIFY(catalogContainsDisplayName(restoredController->catalogBodies(), QStringLiteral("Downloaded Star")));
    QCOMPARE(restoredController->catalogPresetIndex(), 2);
    QCOMPARE(restoredController->catalogUrlText(), starCatalogUrl);
    QCOMPARE(restoredController->deepSkyCatalogPresetIndex(), 2);
    QCOMPARE(restoredController->deepSkyCatalogUrlText(), deepSkyCatalogUrl);
    QTRY_VERIFY(catalogContainsAlias(restoredController->catalogBodies(), QStringLiteral("Custom Galaxy")));

    draft->setProperty("catalogPresetIndex", 0);
    draft->setProperty("deepSkyCatalogPresetIndex", 0);
    QCoreApplication::processEvents();
    QObject* starUseButton = firstObjectWithObjectName(root, QStringLiteral("starCatalogUseButton"));
    QVERIFY(starUseButton != nullptr);
    QVERIFY(activateControl(starUseButton));
    QTRY_VERIFY(!catalogContainsDisplayName(controller->catalogBodies(), QStringLiteral("Downloaded Star")));
    QVERIFY(controller->catalogStatusText().contains("Bundled"));

    QObject* deepSkyUseButton = firstObjectWithObjectName(root, QStringLiteral("deepSkyCatalogUseButton"));
    QVERIFY(deepSkyUseButton != nullptr);
    QVERIFY(activateControl(deepSkyUseButton));
    QTRY_VERIFY(!catalogContainsAlias(controller->catalogBodies(), QStringLiteral("Custom Galaxy")));

    QObject* starClearButton = firstObjectWithObjectName(root, QStringLiteral("starCatalogClearCacheButton"));
    QVERIFY(starClearButton != nullptr);
    QVERIFY(activateControl(starClearButton));
    QTRY_VERIFY(!QFileInfo::exists(m_settings.cachePath(QStringLiteral("star-cache.csv"))));
    QVERIFY(controller->catalogStatusText().contains("Star catalog cache cleared"));

    QObject* deepSkyClearButton = firstObjectWithObjectName(root, QStringLiteral("deepSkyCatalogClearCacheButton"));
    QVERIFY(deepSkyClearButton != nullptr);
    QVERIFY(activateControl(deepSkyClearButton));
    QVERIFY(controller->catalogStatusText().contains("Deep-sky catalog cache cleared"));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

SKYGATE_QML_TEST_MAIN(QmlPreferencesCatalogTests)

#include "QmlPreferencesCatalogTests.moc"
