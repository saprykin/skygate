#include "QmlPreferencesTestSupport.hpp"

#include "skygate/ephemeris/EphemerisDataManifest.hpp"

#include <QSettings>

namespace {

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
        QStringLiteral("skyContext/ephemerisData/installedEarthOrientationVersion"), QStringLiteral("EOP-test")
    );
    settings.setValue(QStringLiteral("skyContext/ephemerisData/installedLeapSecondTablePath"), leapSecondPath);
    settings.setValue(
        QStringLiteral("skyContext/ephemerisData/installedLeapSecondTableVersion"), QStringLiteral("LS-test")
    );
    settings.setValue(QStringLiteral("skyContext/ephemerisData/installedDeltaTDataPath"), deltaTPath);
    settings.setValue(QStringLiteral("skyContext/ephemerisData/installedDeltaTDataVersion"), QStringLiteral("DT-test"));
    settings.setValue(QStringLiteral("skyContext/ephemerisData/dataRevisionToken"), QStringLiteral("de441-test"));
    settings.setValue(QStringLiteral("skyContext/ephemerisData/lastUpdateResult"), QStringLiteral("Installed DE441"));
}

skygate::ephemeris::EphemerisDataManifest minimalUpdateManifest()
{
    skygate::ephemeris::EphemerisDataManifest manifest;
    manifest.profiles.push_back(skygate::ephemeris::EphemerisDataManifestProfile{
        .id = "modern",
        .displayName = "Modern",
        .bundled = true,
        .longRange = false,
        .assetIds = {},
    });
    return manifest;
}

std::unique_ptr<SkyContextController> makeControllerWithManifest(
    const skygate::ephemeris::EphemerisDataManifest& manifest,
    const QString& updateResourceRoot,
    const QString& writableCacheRoot
)
{
    auto starCatalog = skygate::ephemeris::createBundledStarCatalog();
    if (starCatalog == nullptr) {
        return {};
    }
    auto ephemerisEngine = skygate::ephemeris::createEphemerisEngine(*starCatalog);
    if (ephemerisEngine == nullptr) {
        return {};
    }

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
            PreferencesCatalogSection {
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
    auto controller = makeControllerWithManifest(
        manifest,
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
            PreferencesCatalogSection {
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

    QObject* modernStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisModernKernelStatusLabel"));
    QObject* longRangeStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisLongRangeStatusLabel"));
    QObject* eopStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisEarthOrientationStatusLabel"));
    QObject* leapSecondStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisLeapSecondStatusLabel"));
    QObject* deltaTStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisDeltaTStatusLabel"));
    QObject* lastUpdateStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisLastUpdateStatusLabel"));
    QObject* onlineUpdates = firstObjectWithObjectName(root, QStringLiteral("ephemerisDataOnlineUpdatesCheckBox"));
    QObject* updateButton = firstObjectWithObjectName(root, QStringLiteral("ephemerisDataUpdateButton"));
    QVERIFY(modernStatus != nullptr);
    QVERIFY(longRangeStatus != nullptr);
    QVERIFY(eopStatus != nullptr);
    QVERIFY(leapSecondStatus != nullptr);
    QVERIFY(deltaTStatus != nullptr);
    QVERIFY(lastUpdateStatus != nullptr);
    QVERIFY(onlineUpdates != nullptr);
    QVERIFY(updateButton != nullptr);

    QCOMPARE(modernStatus->property("text").toString(), QString("Bundled fallback"));
    QCOMPARE(longRangeStatus->property("text").toString(), QString("Not installed"));
    QCOMPARE(eopStatus->property("text").toString(), QString("Bundled fallback"));
    QCOMPARE(leapSecondStatus->property("text").toString(), QString("Bundled fallback"));
    QCOMPARE(deltaTStatus->property("text").toString(), QString("Bundled fallback"));
    QCOMPARE(lastUpdateStatus->property("text").toString(), QString("Bundled fallback"));
    QVERIFY(onlineUpdates->property("checked").toBool());
    QVERIFY(updateButton->property("enabled").toBool());

    QVERIFY(activateControl(onlineUpdates));
    QTRY_VERIFY(!controller->ephemerisDataOnlineUpdatesEnabled());
    QTRY_VERIFY(!updateButton->property("enabled").toBool());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesCatalogTests::ephemerisDataControlsShowInstalledStateAndClearCache()
{
    const QString kernelPath = m_settings.cachePath(QStringLiteral("de441.bsp"));
    const QString earthOrientationPath = m_settings.cachePath(QStringLiteral("eop.csv"));
    const QString leapSecondPath = m_settings.cachePath(QStringLiteral("leap-seconds.list"));
    const QString deltaTPath = m_settings.cachePath(QStringLiteral("delta-t.csv"));
    QVERIFY(writeFile(kernelPath, QByteArray("kernel")));
    QVERIFY(writeFile(earthOrientationPath, QByteArray("eop")));
    QVERIFY(writeFile(leapSecondPath, QByteArray("leap")));
    QVERIFY(writeFile(deltaTPath, QByteArray("delta")));
    writeInstalledEphemerisSettings(kernelPath, earthOrientationPath, leapSecondPath, deltaTPath);

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
            height: 720
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
        QStringLiteral("PreferencesEphemerisDataInstalledTest.qml")
    );
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    QObject* modernStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisModernKernelStatusLabel"));
    QObject* longRangeStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisLongRangeStatusLabel"));
    QObject* eopStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisEarthOrientationStatusLabel"));
    QObject* leapSecondStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisLeapSecondStatusLabel"));
    QObject* deltaTStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisDeltaTStatusLabel"));
    QObject* lastUpdateStatus = firstObjectWithObjectName(root, QStringLiteral("ephemerisLastUpdateStatusLabel"));
    QObject* updateButton = firstObjectWithObjectName(root, QStringLiteral("ephemerisDataUpdateButton"));
    QObject* clearButton = firstObjectWithObjectName(root, QStringLiteral("ephemerisDataClearCacheButton"));
    QVERIFY(modernStatus != nullptr);
    QVERIFY(longRangeStatus != nullptr);
    QVERIFY(eopStatus != nullptr);
    QVERIFY(leapSecondStatus != nullptr);
    QVERIFY(deltaTStatus != nullptr);
    QVERIFY(lastUpdateStatus != nullptr);
    QVERIFY(updateButton != nullptr);
    QVERIFY(clearButton != nullptr);

    QCOMPARE(modernStatus->property("text").toString(), QString("Installed: DE441-test"));
    QCOMPARE(longRangeStatus->property("text").toString(), QString("Installed: DE441-test"));
    QCOMPARE(eopStatus->property("text").toString(), QString("Installed: EOP-test"));
    QCOMPARE(leapSecondStatus->property("text").toString(), QString("Installed: LS-test"));
    QCOMPARE(deltaTStatus->property("text").toString(), QString("Installed: DT-test"));
    QCOMPARE(lastUpdateStatus->property("text").toString(), QString("Installed DE441"));
    QVERIFY(!updateButton->property("enabled").toBool());

    QVERIFY(activateControl(clearButton));
    QTRY_COMPARE(modernStatus->property("text").toString(), QString("Bundled fallback"));
    QTRY_COMPARE(longRangeStatus->property("text").toString(), QString("Not installed"));
    QTRY_COMPARE(lastUpdateStatus->property("text").toString(), QString("Bundled fallback"));
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
