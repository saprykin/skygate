#include "QmlTestSupport.hpp"

using namespace skygate::ui::tests;

class QmlPreferencesTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void catalogSectionBindsDraftAndControls();
    void settingsDraftApplyAndResetRoundTrip();
    void catalogSectionDownloadsAppliesClearsAndRestoresCatalogs();
    void appearanceThemeSwitchWorks();
    void appearanceCheckboxChangesRender_data();
    void appearanceCheckboxChangesRender();
    void locationCoordinateChangesPropagateToSkyView();
    void saveAndRestoreState();

private:
    QmlSettingsFixture m_settings;
};

void QmlPreferencesTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("QmlPreferencesTests")));
}

void QmlPreferencesTests::init()
{
    m_settings.resetForCurrentTest();
}

void QmlPreferencesTests::catalogSectionBindsDraftAndControls()
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
            SkySettingsDraft {
                id: draft
                skyContextController: skyContext
                Component.onCompleted: resetFromContext()
            }
            PreferencesCatalogSection {
                anchors.fill: parent
                skyContextController: skyContext
                settingsDraft: draft
            }
        }
    )"), QStringLiteral("PreferencesCatalogSectionBehaviorTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    ExposedQuickWindow exposed(root);
    QObject* draft = qvariant_cast<QObject*>(root->property("draft"));
    QVERIFY(draft != nullptr);

    const auto combos = comboBoxesWithCount(root, 3);
    QVERIFY(combos.size() >= 2);
    QObject* catalogCombo = combos.front();
    QCOMPARE(catalogCombo->property("currentIndex").toInt(), 0);

    const auto useButtons = invokableButtonsWithText(root, QStringLiteral("Use catalog"));
    QVERIFY(useButtons.size() >= 2);
    QVERIFY(useButtons.front()->property("enabled").toBool());

    draft->setProperty("catalogPresetIndex", 2);
    QCoreApplication::processEvents();
    QTRY_COMPARE(catalogCombo->property("currentIndex").toInt(), 2);
    QVERIFY(!useButtons.front()->property("enabled").toBool());

    auto* catalogUrlInput = qobject_cast<QQuickItem*>(firstObjectWithProperty(
        root,
        "placeholderText",
        QStringLiteral("https://example.com/skygate-catalog.txt or HYG CSV URL")
    ));
    QVERIFY(catalogUrlInput != nullptr);
    QTRY_VERIFY(catalogUrlInput->isVisible());

    QVERIFY(QMetaObject::invokeMethod(catalogUrlInput, "forceActiveFocus"));
    QVERIFY(QMetaObject::invokeMethod(catalogUrlInput, "selectAll"));
    constexpr const char* kCatalogUrl = "https://example.test/custom-stars.csv";
    commitText(exposed.window(), QString::fromUtf8(kCatalogUrl));
    QTRY_COMPARE(draft->property("catalogUrlText").toString(), QString(kCatalogUrl));

    const auto downloadButtons = invokableButtonsWithText(root, QStringLiteral("Download"));
    QVERIFY(downloadButtons.size() >= 2);
    QVERIFY(downloadButtons.front()->property("enabled").toBool());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesTests::settingsDraftApplyAndResetRoundTrip()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    controller->setLatitudeText(QStringLiteral("47.000000"));
    controller->setLongitudeText(QStringLiteral("8.000000"));
    controller->setElevationText(QStringLiteral("400"));
    controller->overlayLayers()->setProperty("horizon", true);

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createFileComponent(
        engine,
        QStringLiteral("SkySettingsDraft.qml"),
        {{QStringLiteral("skyContextController"), QVariant::fromValue<QObject*>(controller.get())}}
    );
    QVERIFY(object != nullptr);

    QVERIFY(QMetaObject::invokeMethod(object.get(), "resetFromContext"));
    QCOMPARE(object->property("latitudeText").toString(), QString("47.000000"));
    QCOMPARE(object->property("longitudeText").toString(), QString("8.000000"));
    QCOMPARE(object->property("overlayHorizon").toBool(), true);

    object->setProperty("latitudeText", QStringLiteral("12.500000"));
    object->setProperty("longitudeText", QStringLiteral("34.500000"));
    object->setProperty("elevationText", QStringLiteral("88"));
    object->setProperty("locationSourceText", QStringLiteral("Custom"));
    object->setProperty("projectionTypeText", QStringLiteral("AzimuthalEquidistant"));
    object->setProperty("overlayHorizon", false);
    object->setProperty("catalogPresetIndex", 2);
    object->setProperty("catalogUrlText", QStringLiteral("https://example.test/stars.csv"));
    QVERIFY(QMetaObject::invokeMethod(object.get(), "applyToContext"));

    QCOMPARE(controller->latitudeText(), QString("12.500000"));
    QCOMPARE(controller->longitudeText(), QString("34.500000"));
    QCOMPARE(controller->elevationText(), QString("88.0"));
    QCOMPARE(controller->projectionTypeText(), QString("AzimuthalEquidistant"));
    QCOMPARE(controller->overlayLayers()->property("horizon").toBool(), false);
    QCOMPARE(controller->catalogPresetIndex(), 2);
    QCOMPARE(controller->catalogUrlText(), QString("https://example.test/stars.csv"));

    object->setProperty("latitudeText", QStringLiteral("99.000000"));
    QVERIFY(QMetaObject::invokeMethod(object.get(), "resetFromContext"));
    QCOMPARE(object->property("latitudeText").toString(), QString("12.500000"));
    QCOMPARE(object->property("catalogUrlText").toString(), QString("https://example.test/stars.csv"));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesTests::catalogSectionDownloadsAppliesClearsAndRestoresCatalogs()
{
    const QString starCatalogPath = m_settings.cachePath(QStringLiteral("download-stars.csv"));
    const QString deepSkyCatalogPath = m_settings.cachePath(QStringLiteral("download-dso.csv"));
    const QByteArray starCatalogPayload =
        "id,hip,proper,ra,dec,mag\n"
        "1,900001,Downloaded Star,6.7525,-16.7161,1.0\n";
    const QByteArray deepSkyCatalogPayload =
        "Name;Type;RA;Dec;Const;MajAx;MinAx;PosAng;B-Mag;V-Mag;J-Mag;H-Mag;K-Mag;"
        "SurfBr;Hubble;Cstar U-Mag;Cstar B-Mag;Cstar V-Mag;M;NGC;IC;Cstar Names;"
        "Identifiers;Common names;NED notes;OpenNGC notes\n"
        "NGC0999;G;00:42:44.35;+41:16:08.6;And;12.0;6.0;35;;8.10;;;;;"
        "SA(s)b;;;;;0999;;;PGC 9999;Custom Galaxy;;\n";
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
            SkySettingsDraft {
                id: draft
                skyContextController: skyContext
                Component.onCompleted: resetFromContext()
            }
            PreferencesCatalogSection {
                anchors.fill: parent
                skyContextController: skyContext
                settingsDraft: draft
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
    auto downloadButtons = invokableButtonsWithText(root, QStringLiteral("Download"));
    QVERIFY(downloadButtons.size() >= 2);
    QVERIFY(activateControl(downloadButtons.front()));
    QTRY_VERIFY(!controller->downloadingCatalog() && !controller->catalogProcessing());
    QTRY_VERIFY(catalogContainsDisplayName(controller->catalogBodies(), QStringLiteral("Downloaded Star")));
    QVERIFY(QFileInfo::exists(m_settings.cachePath(QStringLiteral("star-cache.csv"))));

    draft->setProperty("deepSkyCatalogPresetIndex", 2);
    draft->setProperty("deepSkyCatalogUrlText", deepSkyCatalogUrl);
    QCoreApplication::processEvents();
    downloadButtons = invokableButtonsWithText(root, QStringLiteral("Download"));
    QVERIFY(downloadButtons.size() >= 2);
    QVERIFY(activateControl(downloadButtons.at(1)));
    QTRY_VERIFY(!controller->downloadingCatalog() && !controller->catalogProcessing());
    QTRY_VERIFY(catalogContainsAlias(controller->catalogBodies(), QStringLiteral("Custom Galaxy")));

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

    draft->setProperty("catalogPresetIndex", 0);
    draft->setProperty("deepSkyCatalogPresetIndex", 0);
    QCoreApplication::processEvents();
    const auto useButtons = invokableButtonsWithText(root, QStringLiteral("Use catalog"));
    QVERIFY(useButtons.size() >= 2);
    QVERIFY(activateControl(useButtons.front()));
    QTRY_VERIFY(!catalogContainsDisplayName(controller->catalogBodies(), QStringLiteral("Downloaded Star")));
    QVERIFY(controller->catalogStatusText().contains("Bundled"));

    QVERIFY(activateControl(useButtons.at(1)));
    QTRY_VERIFY(!catalogContainsAlias(controller->catalogBodies(), QStringLiteral("Custom Galaxy")));

    const auto clearButtons = invokableButtonsWithText(root, QStringLiteral("Clear Catalog Cache"));
    QVERIFY(clearButtons.size() >= 2);
    QVERIFY(activateControl(clearButtons.front()));
    QTRY_VERIFY(!QFileInfo::exists(m_settings.cachePath(QStringLiteral("star-cache.csv"))));
    QVERIFY(controller->catalogStatusText().contains("Star catalog cache cleared"));

    QVERIFY(activateControl(clearButtons.at(1)));
    QVERIFY(controller->catalogStatusText().contains("Deep-sky catalog cache cleared"));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesTests::appearanceThemeSwitchWorks()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    const QVariantList themeOptions = controller->themeOptions();
    QVERIFY(themeOptions.size() >= 2);
    const QString nextThemeId = themeOptions.at(1).toMap().value("id").toString();
    QVERIFY(!nextThemeId.isEmpty());
    const QColor originalStarColor = controller->renderTheme().bodyStar;

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createInlineComponent(engine, QStringLiteral(R"(
        import QtQuick
        Item {
            id: root
            width: 900
            height: 420
            property alias draft: draft
            SkySettingsDraft {
                id: draft
                skyContextController: skyContext
                Component.onCompleted: resetFromContext()
            }
            PreferencesAppearanceSection {
                anchors.fill: parent
                settingsDraft: draft
            }
        }
    )"), QStringLiteral("PreferencesAppearanceThemeTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    ExposedQuickWindow exposed(root);
    (void)exposed;
    QObject* draft = qvariant_cast<QObject*>(root->property("draft"));
    QVERIFY(draft != nullptr);

    const auto themeCombos = comboBoxesWithCount(root, themeOptions.size());
    QCOMPARE(themeCombos.size(), 1);
    themeCombos.front()->setProperty("currentIndex", 1);
    QVERIFY(QMetaObject::invokeMethod(themeCombos.front(), "activated", Q_ARG(int, 1)));
    QCOMPARE(draft->property("themeId").toString(), nextThemeId);
    QVERIFY(QMetaObject::invokeMethod(draft, "applyToContext"));
    QCOMPARE(controller->themeId(), nextThemeId);
    QVERIFY(controller->renderTheme().bodyStar != originalStarColor);
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesTests::appearanceCheckboxChangesRender_data()
{
    QTest::addColumn<int>("checkboxIndex");
    QTest::addColumn<QString>("name");

    QTest::newRow("horizon") << 0 << QStringLiteral("Horizon");
    QTest::newRow("alt-az-grid") << 1 << QStringLiteral("Alt/Az grid");
    QTest::newRow("constellation-lines") << 2 << QStringLiteral("Constellation lines");
    QTest::newRow("constellation-labels") << 3 << QStringLiteral("Constellation labels");
    QTest::newRow("solar-system-labels") << 4 << QStringLiteral("Solar system labels");
    QTest::newRow("deep-sky-objects") << 5 << QStringLiteral("Deep sky objects");
    QTest::newRow("deep-sky-labels") << 6 << QStringLiteral("Deep sky labels");
    QTest::newRow("ecliptic") << 7 << QStringLiteral("Ecliptic");
    QTest::newRow("celestial-equator") << 8 << QStringLiteral("Celestial equator");
    QTest::newRow("circumpolar-boundary") << 9 << QStringLiteral("Circumpolar boundary");
}

void QmlPreferencesTests::appearanceCheckboxChangesRender()
{
    QFETCH(int, checkboxIndex);
    QFETCH(QString, name);

    auto controller = makeController();
    QVERIFY(controller != nullptr);
    controller->setMagnitudeCutoff(8.0);
    controller->setViewCenter(45.0, 180.0);
    auto sceneModel = makeSceneModel(*controller);
    QVERIFY(sceneModel != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller, sceneModel.get());

    const QmlWarningScope warnings;
    auto object = createInlineComponent(engine, QStringLiteral(R"(
        import QtQuick
        Item {
            id: root
            width: 900
            height: 420
            property alias draft: draft
            SkySettingsDraft {
                id: draft
                skyContextController: skyContext
                Component.onCompleted: resetFromContext()
            }
            PreferencesAppearanceSection {
                anchors.fill: parent
                settingsDraft: draft
            }
        }
    )"), QStringLiteral("PreferencesAppearanceCheckboxTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    ExposedQuickWindow exposed(root);
    (void)exposed;
    QObject* draft = qvariant_cast<QObject*>(root->property("draft"));
    QVERIFY(draft != nullptr);

    const auto boxes = checkBoxes(root);
    QVERIFY(boxes.size() >= 10);
    const SkyViewRenderSignature baseline = renderSignature(*controller, *sceneModel);

    const bool previousChecked = boxes.at(checkboxIndex)->property("checked").toBool();
    boxes.at(checkboxIndex)->setProperty("checked", !previousChecked);
    QVERIFY(QMetaObject::invokeMethod(boxes.at(checkboxIndex), "toggled"));
    QVERIFY(QMetaObject::invokeMethod(draft, "applyToContext"));
    const SkyViewRenderSignature changed = renderSignature(*controller, *sceneModel);
    QVERIFY2(
        changed.differsFrom(baseline),
        qPrintable(QString("Changing %1 did not alter the sky-view render signature").arg(name))
    );
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesTests::locationCoordinateChangesPropagateToSkyView()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    auto sceneModel = makeSceneModel(*controller);
    QVERIFY(sceneModel != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller, sceneModel.get());

    const QmlWarningScope warnings;
    auto object = createInlineComponent(engine, QStringLiteral(R"(
        import QtQuick
        Item {
            id: root
            width: 900
            height: 420
            property alias draft: draft
            SkySettingsDraft {
                id: draft
                skyContextController: skyContext
                Component.onCompleted: resetFromContext()
            }
            PreferencesSkySection {
                anchors.fill: parent
                settingsDraft: draft
            }
        }
    )"), QStringLiteral("PreferencesLocationTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    ExposedQuickWindow exposed(root);
    QObject* draft = qvariant_cast<QObject*>(root->property("draft"));
    QVERIFY(draft != nullptr);

    const SkyViewRenderSignature baseline = renderSignature(*controller, *sceneModel);
    const auto latitudeInputs = objectsWithPlaceholder(root, QStringLiteral("-90..90"));
    const auto longitudeInputs = objectsWithPlaceholder(root, QStringLiteral("-180..180"));
    const auto elevationInputs = objectsWithPlaceholder(root, QStringLiteral("meters"));
    QCOMPARE(latitudeInputs.size(), 1);
    QCOMPARE(longitudeInputs.size(), 1);
    QCOMPARE(elevationInputs.size(), 1);

    replaceText(exposed.window(), qobject_cast<QQuickItem*>(latitudeInputs.front()), QStringLiteral("12.5"));
    replaceText(exposed.window(), qobject_cast<QQuickItem*>(longitudeInputs.front()), QStringLiteral("34.5"));
    replaceText(exposed.window(), qobject_cast<QQuickItem*>(elevationInputs.front()), QStringLiteral("88"));
    QCOMPARE(draft->property("locationSourceText").toString(), QString("Custom"));
    QVERIFY(QMetaObject::invokeMethod(draft, "applyToContext"));

    QCOMPARE(controller->locationSourceText(), QString("Custom"));
    QCOMPARE(controller->latitudeText(), QString("12.500000"));
    QCOMPARE(controller->longitudeText(), QString("34.500000"));
    QCOMPARE(controller->elevationText(), QString("88.0"));
    const SkyViewRenderSignature changed = renderSignature(*controller, *sceneModel);
    QVERIFY(changed.differsFrom(baseline));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesTests::saveAndRestoreState()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createFileComponent(
        engine,
        QStringLiteral("SkySettingsDraft.qml"),
        {{QStringLiteral("skyContextController"), QVariant::fromValue<QObject*>(controller.get())}}
    );
    QVERIFY(object != nullptr);
    QVERIFY(QMetaObject::invokeMethod(object.get(), "resetFromContext"));
    object->setProperty("latitudeText", QStringLiteral("12.500000"));
    object->setProperty("longitudeText", QStringLiteral("34.500000"));
    object->setProperty("elevationText", QStringLiteral("88"));
    object->setProperty("locationSourceText", QStringLiteral("Custom"));
    object->setProperty("projectionTypeText", QStringLiteral("Perspective"));
    object->setProperty("themeId", QStringLiteral("night-vision"));
    object->setProperty("overlayHorizon", false);
    object->setProperty("overlayEcliptic", true);
    object->setProperty("catalogPresetIndex", 2);
    object->setProperty("catalogUrlText", QStringLiteral("file:///tmp/stars.csv"));
    object->setProperty("deepSkyCatalogPresetIndex", 2);
    object->setProperty("deepSkyCatalogUrlText", QStringLiteral("file:///tmp/dso.csv"));
    QVERIFY(QMetaObject::invokeMethod(object.get(), "applyToContext"));
    QVERIFY(controller->saveSettings());

    auto restoredController = makeController();
    QVERIFY(restoredController != nullptr);
    QCOMPARE(restoredController->latitudeText(), QString("12.500000"));
    QCOMPARE(restoredController->longitudeText(), QString("34.500000"));
    QCOMPARE(restoredController->elevationText(), QString("88.0"));
    QCOMPARE(restoredController->locationSourceText(), QString("Custom"));
    QCOMPARE(restoredController->projectionTypeText(), QString("Perspective"));
    QCOMPARE(restoredController->themeId(), QString("night-vision"));
    QCOMPARE(restoredController->overlayLayerVisibility().horizon, false);
    QCOMPARE(restoredController->overlayLayerVisibility().ecliptic, true);
    QCOMPARE(restoredController->catalogPresetIndex(), 2);
    QCOMPARE(restoredController->catalogUrlText(), QString("file:///tmp/stars.csv"));
    QCOMPARE(restoredController->deepSkyCatalogPresetIndex(), 2);
    QCOMPARE(restoredController->deepSkyCatalogUrlText(), QString("file:///tmp/dso.csv"));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

SKYGATE_QML_TEST_MAIN(QmlPreferencesTests)

#include "QmlPreferencesTests.moc"
