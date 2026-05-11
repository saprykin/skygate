#include "QmlPreferencesTestSupport.hpp"

class QmlPreferencesCatalogTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void catalogSectionBindsDraftAndControls();
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
    auto object = createInlineComponent(engine, QStringLiteral(R"(
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
    )"), QStringLiteral("PreferencesCatalogSectionBehaviorTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    ExposedQuickWindow exposed(root);
    QObject* draft = qvariant_cast<QObject*>(root->property("draft"));
    QVERIFY(draft != nullptr);

    QObject* catalogCombo = firstObjectWithObjectName(
        root,
        QStringLiteral("starCatalogPresetCombo")
    );
    QVERIFY(catalogCombo != nullptr);
    QCOMPARE(catalogCombo->property("currentIndex").toInt(), 0);

    QObject* useButton = firstObjectWithObjectName(root, QStringLiteral("starCatalogUseButton"));
    QVERIFY(useButton != nullptr);
    QVERIFY(useButton->property("enabled").toBool());

    draft->setProperty("catalogPresetIndex", 2);
    QCoreApplication::processEvents();
    QTRY_COMPARE(catalogCombo->property("currentIndex").toInt(), 2);
    QVERIFY(!useButton->property("enabled").toBool());

    auto* catalogUrlInput = firstQuickItemWithObjectName(
        root,
        QStringLiteral("starCatalogUrlInput")
    );
    QVERIFY(catalogUrlInput != nullptr);
    QTRY_VERIFY(catalogUrlInput->isVisible());

    QVERIFY(QMetaObject::invokeMethod(catalogUrlInput, "forceActiveFocus"));
    QVERIFY(QMetaObject::invokeMethod(catalogUrlInput, "selectAll"));
    constexpr const char* kCatalogUrl = "https://example.test/custom-stars.csv";
    commitText(exposed.window(), QString::fromUtf8(kCatalogUrl));
    QTRY_COMPARE(draft->property("catalogUrlText").toString(), QString(kCatalogUrl));

    QObject* downloadButton = firstObjectWithObjectName(
        root,
        QStringLiteral("starCatalogDownloadButton")
    );
    QVERIFY(downloadButton != nullptr);
    QVERIFY(downloadButton->property("enabled").toBool());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesCatalogTests::catalogSectionDownloadsAppliesClearsAndRestoresCatalogs()
{
    const QString starCatalogPath = m_settings.cachePath(QStringLiteral("download-stars.csv"));
    const QString deepSkyCatalogPath = m_settings.cachePath(QStringLiteral("download-dso.csv"));
    const QByteArray starCatalogPayload = sampleHygCsvPayload(
        {1, 900001, "Downloaded Star", "6.7525", "-16.7161", "1.0"}
    );
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
    auto object = createInlineComponent(engine, QStringLiteral(R"(
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
    )"), QStringLiteral("PreferencesCatalogDownloadTest.qml"));
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
    QObject* starDownloadButton = firstObjectWithObjectName(
        root,
        QStringLiteral("starCatalogDownloadButton")
    );
    QVERIFY(starDownloadButton != nullptr);
    QVERIFY(activateControl(starDownloadButton));
    QTRY_VERIFY(!controller->downloadingCatalog() && !controller->catalogProcessing());
    QTRY_VERIFY(catalogContainsDisplayName(
        controller->catalogBodies(),
        QStringLiteral("Downloaded Star")
    ));
    QVERIFY(QFileInfo::exists(m_settings.cachePath(QStringLiteral("star-cache.csv"))));

    draft->setProperty("deepSkyCatalogPresetIndex", 2);
    draft->setProperty("deepSkyCatalogUrlText", deepSkyCatalogUrl);
    QCoreApplication::processEvents();
    QObject* deepSkyDownloadButton = firstObjectWithObjectName(
        root,
        QStringLiteral("deepSkyCatalogDownloadButton")
    );
    QVERIFY(deepSkyDownloadButton != nullptr);
    QVERIFY(activateControl(deepSkyDownloadButton));
    QTRY_VERIFY(!controller->downloadingCatalog() && !controller->catalogProcessing());
    QTRY_VERIFY(catalogContainsAlias(controller->catalogBodies(), QStringLiteral("Custom Galaxy")));
    QVERIFY(QFileInfo::exists(m_settings.cachePath(QStringLiteral("deep-sky-cache.csv"))));

    QVERIFY(controller->saveSettings());
    auto restoredController = makeController();
    QVERIFY(restoredController != nullptr);
    QTRY_VERIFY(catalogContainsDisplayName(
        restoredController->catalogBodies(),
        QStringLiteral("Downloaded Star")
    ));
    QCOMPARE(restoredController->catalogPresetIndex(), 2);
    QCOMPARE(restoredController->catalogUrlText(), starCatalogUrl);
    QCOMPARE(restoredController->deepSkyCatalogPresetIndex(), 2);
    QCOMPARE(restoredController->deepSkyCatalogUrlText(), deepSkyCatalogUrl);
    QTRY_VERIFY(catalogContainsAlias(
        restoredController->catalogBodies(),
        QStringLiteral("Custom Galaxy")
    ));

    draft->setProperty("catalogPresetIndex", 0);
    draft->setProperty("deepSkyCatalogPresetIndex", 0);
    QCoreApplication::processEvents();
    QObject* starUseButton = firstObjectWithObjectName(
        root,
        QStringLiteral("starCatalogUseButton")
    );
    QVERIFY(starUseButton != nullptr);
    QVERIFY(activateControl(starUseButton));
    QTRY_VERIFY(!catalogContainsDisplayName(
        controller->catalogBodies(),
        QStringLiteral("Downloaded Star")
    ));
    QVERIFY(controller->catalogStatusText().contains("Bundled"));

    QObject* deepSkyUseButton = firstObjectWithObjectName(
        root,
        QStringLiteral("deepSkyCatalogUseButton")
    );
    QVERIFY(deepSkyUseButton != nullptr);
    QVERIFY(activateControl(deepSkyUseButton));
    QTRY_VERIFY(!catalogContainsAlias(
        controller->catalogBodies(),
        QStringLiteral("Custom Galaxy")
    ));

    QObject* starClearButton = firstObjectWithObjectName(
        root,
        QStringLiteral("starCatalogClearCacheButton")
    );
    QVERIFY(starClearButton != nullptr);
    QVERIFY(activateControl(starClearButton));
    QTRY_VERIFY(!QFileInfo::exists(m_settings.cachePath(QStringLiteral("star-cache.csv"))));
    QVERIFY(controller->catalogStatusText().contains("Star catalog cache cleared"));

    QObject* deepSkyClearButton = firstObjectWithObjectName(
        root,
        QStringLiteral("deepSkyCatalogClearCacheButton")
    );
    QVERIFY(deepSkyClearButton != nullptr);
    QVERIFY(activateControl(deepSkyClearButton));
    QVERIFY(controller->catalogStatusText().contains("Deep-sky catalog cache cleared"));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

SKYGATE_QML_TEST_MAIN(QmlPreferencesCatalogTests)

#include "QmlPreferencesCatalogTests.moc"
