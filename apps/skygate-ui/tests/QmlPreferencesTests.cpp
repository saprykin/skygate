#include "QmlTestSupport.hpp"
#include "SkyTimeController.hpp"

using namespace skygate::ui::tests;

class QmlPreferencesTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void catalogSectionBindsDraftAndControls();
    void preferencesDraftApplyAndResetRoundTrip();
    void catalogSectionDownloadsAppliesClearsAndRestoresCatalogs();
    void generalLoggingControlsBindDraft();
    void appearanceThemeSwitchWorks();
    void appearanceCheckboxChangesRender_data();
    void appearanceCheckboxChangesRender();
    void locationCoordinateChangesPropagateToSkyView();
    void timeZonePickerFiltersSelectsAndApplies();
    void preferencesDraftCurrentDeviceApplyAndSyncWork();
    void preferencesWindowShellNavigatesAppliesAndCloses();
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

void QmlPreferencesTests::preferencesDraftApplyAndResetRoundTrip()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    controller->setLatitudeText(QStringLiteral("47.000000"));
    controller->setLongitudeText(QStringLiteral("8.000000"));
    controller->setElevationText(QStringLiteral("400"));
    QVERIFY(controller->timeController()->setTimeZoneId(QStringLiteral("Europe/Zurich")));
    controller->overlayLayers()->setProperty("horizon", true);
    controller->setLogToTerminal(false);
    controller->setLogToFile(true);
    controller->setLogFilePath(QStringLiteral("/tmp/skygate-qml-test.log"));

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createFileComponent(
        engine,
        QStringLiteral("PreferencesDraft.qml"),
        {{QStringLiteral("skyContextController"), QVariant::fromValue<QObject*>(controller.get())}}
    );
    QVERIFY(object != nullptr);

    QVERIFY(QMetaObject::invokeMethod(object.get(), "resetFromContext"));
    QCOMPARE(object->property("latitudeText").toString(), QString("47.000000"));
    QCOMPARE(object->property("longitudeText").toString(), QString("8.000000"));
    QCOMPARE(object->property("timeZoneId").toString(), QString("Europe/Zurich"));
    QVERIFY(object->property("timeZoneDisplayText").toString().contains("Europe/Zurich"));
    QCOMPARE(object->property("overlayHorizon").toBool(), true);
    QCOMPARE(object->property("logToTerminal").toBool(), false);
    QCOMPARE(object->property("logToFile").toBool(), true);
    QCOMPARE(
        object->property("logFilePath").toString(),
        QString("/tmp/skygate-qml-test.log")
    );

    object->setProperty("latitudeText", QStringLiteral("12.500000"));
    object->setProperty("longitudeText", QStringLiteral("34.500000"));
    object->setProperty("elevationText", QStringLiteral("88"));
    object->setProperty("locationSourceText", QStringLiteral("Custom"));
    object->setProperty("timeZoneId", QStringLiteral("Asia/Bishkek"));
    object->setProperty("projectionTypeText", QStringLiteral("AzimuthalEquidistant"));
    object->setProperty("overlayHorizon", false);
    object->setProperty("catalogPresetIndex", 2);
    object->setProperty("catalogUrlText", QStringLiteral("https://example.test/stars.csv"));
    object->setProperty("logToTerminal", true);
    object->setProperty("logToFile", false);
    object->setProperty("logFilePath", QStringLiteral("/tmp/skygate-qml-test-updated.log"));
    QVERIFY(QMetaObject::invokeMethod(object.get(), "applyToContext"));

    QCOMPARE(controller->latitudeText(), QString("12.500000"));
    QCOMPARE(controller->longitudeText(), QString("34.500000"));
    QCOMPARE(controller->elevationText(), QString("88.0"));
    QCOMPARE(controller->timeController()->timeZoneId(), QString("Asia/Bishkek"));
    QCOMPARE(controller->projectionTypeText(), QString("AzimuthalEquidistant"));
    QCOMPARE(controller->overlayLayers()->property("horizon").toBool(), false);
    QCOMPARE(controller->catalogPresetIndex(), 2);
    QCOMPARE(controller->catalogUrlText(), QString("https://example.test/stars.csv"));
    QCOMPARE(controller->logToTerminal(), true);
    QCOMPARE(controller->logToFile(), false);
    QCOMPARE(controller->logFilePath(), QString("/tmp/skygate-qml-test-updated.log"));

    object->setProperty("latitudeText", QStringLiteral("99.000000"));
    QVERIFY(QMetaObject::invokeMethod(object.get(), "resetFromContext"));
    QCOMPARE(object->property("latitudeText").toString(), QString("12.500000"));
    QCOMPARE(
        object->property("catalogUrlText").toString(),
        QString("https://example.test/stars.csv")
    );
    QCOMPARE(object->property("timeZoneId").toString(), QString("Asia/Bishkek"));
    QCOMPARE(
        object->property("logFilePath").toString(),
        QString("/tmp/skygate-qml-test-updated.log")
    );
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
    auto downloadButtons = invokableButtonsWithText(root, QStringLiteral("Download"));
    QVERIFY(downloadButtons.size() >= 2);
    QVERIFY(activateControl(downloadButtons.front()));
    QTRY_VERIFY(!controller->downloadingCatalog() && !controller->catalogProcessing());
    QTRY_VERIFY(catalogContainsDisplayName(
        controller->catalogBodies(),
        QStringLiteral("Downloaded Star")
    ));
    QVERIFY(QFileInfo::exists(m_settings.cachePath(QStringLiteral("star-cache.csv"))));

    draft->setProperty("deepSkyCatalogPresetIndex", 2);
    draft->setProperty("deepSkyCatalogUrlText", deepSkyCatalogUrl);
    QCoreApplication::processEvents();
    downloadButtons = invokableButtonsWithText(root, QStringLiteral("Download"));
    QVERIFY(downloadButtons.size() >= 2);
    QVERIFY(activateControl(downloadButtons.at(1)));
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
    const auto useButtons = invokableButtonsWithText(root, QStringLiteral("Use catalog"));
    QVERIFY(useButtons.size() >= 2);
    QVERIFY(activateControl(useButtons.front()));
    QTRY_VERIFY(!catalogContainsDisplayName(
        controller->catalogBodies(),
        QStringLiteral("Downloaded Star")
    ));
    QVERIFY(controller->catalogStatusText().contains("Bundled"));

    QVERIFY(activateControl(useButtons.at(1)));
    QTRY_VERIFY(!catalogContainsAlias(
        controller->catalogBodies(),
        QStringLiteral("Custom Galaxy")
    ));

    const auto clearButtons = invokableButtonsWithText(root, QStringLiteral("Clear Catalog Cache"));
    QVERIFY(clearButtons.size() >= 2);
    QVERIFY(activateControl(clearButtons.front()));
    QTRY_VERIFY(!QFileInfo::exists(m_settings.cachePath(QStringLiteral("star-cache.csv"))));
    QVERIFY(controller->catalogStatusText().contains("Star catalog cache cleared"));

    QVERIFY(activateControl(clearButtons.at(1)));
    QVERIFY(controller->catalogStatusText().contains("Deep-sky catalog cache cleared"));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesTests::generalLoggingControlsBindDraft()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    controller->setLogToTerminal(true);
    controller->setLogToFile(false);

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
            PreferencesDraft {
                id: draft
                skyContextController: skyContext
                Component.onCompleted: resetFromContext()
            }
            PreferencesGeneralSection {
                anchors.fill: parent
                preferencesDraft: draft
            }
        }
    )"), QStringLiteral("PreferencesGeneralLoggingTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    ExposedQuickWindow exposed(root);
    (void)exposed;
    QObject* draft = qvariant_cast<QObject*>(root->property("draft"));
    QVERIFY(draft != nullptr);

    const auto boxes = checkBoxes(root);
    QCOMPARE(boxes.size(), 2);
    boxes.at(0)->setProperty("checked", false);
    QVERIFY(QMetaObject::invokeMethod(boxes.at(0), "toggled"));
    boxes.at(1)->setProperty("checked", true);
    QVERIFY(QMetaObject::invokeMethod(boxes.at(1), "toggled"));
    QCOMPARE(draft->property("logToTerminal").toBool(), false);
    QCOMPARE(draft->property("logToFile").toBool(), true);

    auto* logFileInput = qobject_cast<QQuickItem*>(firstObjectWithProperty(
        root,
        "objectName",
        QStringLiteral("logFilePathField")
    ));
    QVERIFY(logFileInput != nullptr);
    QVERIFY2(logFileInput->width() >= 260.0, "Log file field should stay wide enough to read paths");
    logFileInput->setProperty("text", QStringLiteral("/tmp/skygate-qml-preferences.log"));
    QVERIFY(QMetaObject::invokeMethod(logFileInput, "editingFinished"));
    QTRY_COMPARE(
        draft->property("logFilePath").toString(),
        QString("/tmp/skygate-qml-preferences.log")
    );

    const auto browseButtons = invokableButtonsWithText(root, QStringLiteral("Browse..."));
    QCOMPARE(browseButtons.size(), 1);
    QVERIFY(browseButtons.front()->property("enabled").toBool());

    QVERIFY(QMetaObject::invokeMethod(draft, "applyToContext"));
    QCOMPARE(controller->logToTerminal(), false);
    QCOMPARE(controller->logToFile(), true);
    QCOMPARE(controller->logFilePath(), QString("/tmp/skygate-qml-preferences.log"));
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
            PreferencesDraft {
                id: draft
                skyContextController: skyContext
                Component.onCompleted: resetFromContext()
            }
            PreferencesAppearanceSection {
                anchors.fill: parent
                preferencesDraft: draft
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

    const QmlWarningScope warnings(QmlWarningScope::Forwarding::Disabled);
    auto object = createInlineComponent(engine, QStringLiteral(R"(
        import QtQuick
        Item {
            id: root
            width: 900
            height: 420
            property alias draft: draft
            PreferencesDraft {
                id: draft
                skyContextController: skyContext
                Component.onCompleted: resetFromContext()
            }
            PreferencesAppearanceSection {
                anchors.fill: parent
                preferencesDraft: draft
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
            PreferencesDraft {
                id: draft
                skyContextController: skyContext
                Component.onCompleted: resetFromContext()
            }
            PreferencesSkySection {
                anchors.fill: parent
                preferencesDraft: draft
            }
        }
    )"), QStringLiteral("PreferencesLocationTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    ExposedQuickWindow exposed(root);
    QObject* draft = qvariant_cast<QObject*>(root->property("draft"));
    QVERIFY(draft != nullptr);

    renderSignature(*controller, *sceneModel);
    const std::uint64_t baselineSnapshotGeneration = sceneModel->snapshotGeneration();
    const auto latitudeInputs = objectsWithPlaceholder(root, QStringLiteral("-90..90"));
    const auto longitudeInputs = objectsWithPlaceholder(root, QStringLiteral("-180..180"));
    const auto elevationInputs = objectsWithPlaceholder(root, QStringLiteral("meters"));
    QCOMPARE(latitudeInputs.size(), 1);
    QCOMPARE(longitudeInputs.size(), 1);
    QCOMPARE(elevationInputs.size(), 1);

    replaceText(
        exposed.window(),
        qobject_cast<QQuickItem*>(latitudeInputs.front()),
        QStringLiteral("12.5")
    );
    replaceText(
        exposed.window(),
        qobject_cast<QQuickItem*>(longitudeInputs.front()),
        QStringLiteral("34.5")
    );
    replaceText(
        exposed.window(),
        qobject_cast<QQuickItem*>(elevationInputs.front()),
        QStringLiteral("88")
    );
    QCOMPARE(draft->property("locationSourceText").toString(), QString("Custom"));
    QVERIFY(QMetaObject::invokeMethod(draft, "applyToContext"));

    QCOMPARE(controller->locationSourceText(), QString("Custom"));
    QCOMPARE(controller->latitudeText(), QString("12.500000"));
    QCOMPARE(controller->longitudeText(), QString("34.500000"));
    QCOMPARE(controller->elevationText(), QString("88.0"));
    QTRY_VERIFY(sceneModel->snapshotGeneration() > baselineSnapshotGeneration);
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesTests::timeZonePickerFiltersSelectsAndApplies()
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
            PreferencesSkySection {
                anchors.fill: parent
                preferencesDraft: draft
            }
        }
    )"), QStringLiteral("PreferencesTimeZonePickerTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    ExposedQuickWindow exposed(root);
    QObject* draft = qvariant_cast<QObject*>(root->property("draft"));
    QVERIFY(draft != nullptr);

    auto* pickerMouse = qobject_cast<QQuickItem*>(
        firstObjectWithObjectName(root, QStringLiteral("timeZonePickerMouse"))
    );
    QVERIFY(pickerMouse != nullptr);
    const QPointF pickerCenter = pickerMouse->mapToScene(
        QPointF(pickerMouse->width() / 2.0, pickerMouse->height() / 2.0)
    );
    QTest::mouseClick(exposed.window(), Qt::LeftButton, Qt::NoModifier, pickerCenter.toPoint());

    const auto searchInputs = objectsWithPlaceholder(
        root,
        QStringLiteral("Filter by timezone, label, or offset")
    );
    QCOMPARE(searchInputs.size(), 1);
    auto* searchInput = qobject_cast<QQuickItem*>(searchInputs.front());
    QVERIFY(searchInput != nullptr);
    replaceText(exposed.window(), searchInput, QStringLiteral("bishkek"));
    QTRY_VERIFY(firstVisibleItemWithText(root, QStringLiteral("Asia/Bishkek")) != nullptr);

    auto* delegateMouse = qobject_cast<QQuickItem*>(
        firstObjectWithObjectName(root, QStringLiteral("timeZoneDelegateMouse_Asia/Bishkek"))
    );
    QVERIFY(delegateMouse != nullptr);
    const QPointF delegateCenter = delegateMouse->mapToScene(
        QPointF(delegateMouse->width() / 2.0, delegateMouse->height() / 2.0)
    );
    QTest::mouseClick(exposed.window(), Qt::LeftButton, Qt::NoModifier, delegateCenter.toPoint());

    QTRY_COMPARE(draft->property("timeZoneId").toString(), QString("Asia/Bishkek"));
    QVERIFY(draft->property("timeZoneDisplayText").toString().contains("UTC+06:00"));
    QVERIFY(QMetaObject::invokeMethod(draft, "applyToContext"));
    QCOMPARE(controller->timeController()->timeZoneId(), QString("Asia/Bishkek"));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesTests::preferencesDraftCurrentDeviceApplyAndSyncWork()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(SKYGATE_QML_SOURCE_DIR));

    const QmlWarningScope warnings;
    auto object = createInlineComponent(engine, QStringLiteral(R"(
        import QtQuick
        Item {
            id: root
            property alias draft: draft
            property alias fakeController: fakeController
            QtObject {
                id: overlayLayers
                property bool horizon: true
                property bool altAzGrid: true
                property bool constellationLines: true
                property bool constellationLabels: true
                property bool ecliptic: false
                property bool celestialEquator: false
                property bool circumpolarBoundary: false
                property bool solarSystemLabels: true
                property bool deepSkyObjects: true
                property bool deepSkyLabels: true
            }
            QtObject {
                id: fakeTime
                property string timeZoneId: "Europe/Zurich"
                property string timeZoneDetailText: "CET - UTC+01:00"
                property var timeZoneCatalogModel: null
                function setTimeZoneId(value) {
                    timeZoneId = value
                    return true
                }
            }
            QtObject {
                id: fakeController
                property string latitudeText: "47.376900"
                property string longitudeText: "8.541700"
                property string elevationText: "408.0"
                property string locationSourceText: "Custom"
                property string selectedCityId: ""
                property string selectedCityDisplayText: ""
                property string projectionTypeText: "Stereographic"
                property string themeId: "default"
                property bool logToTerminal: true
                property bool logToFile: false
                property string logFilePath: "/tmp/fake-skygate.log"
                property var time: fakeTime
                property var overlayLayers: overlayLayers
                property int lastCatalogPresetIndex: -1
                property string lastCatalogUrlText: ""
                property int lastDeepSkyCatalogPresetIndex: -1
                property string lastDeepSkyCatalogUrlText: ""
                property string lastLocationSourceText: ""
                property string lastElevationText: ""
                property int refreshCurrentLocationCount: 0
                function catalogPresetIndex() { return 0 }
                function catalogUrlText() { return "" }
                function deepSkyCatalogPresetIndex() { return 0 }
                function deepSkyCatalogUrlText() { return "" }
                function setLocationSourceText(value) {
                    lastLocationSourceText = value
                    locationSourceText = value
                }
                function setSelectedCityId(value) { selectedCityId = value }
                function setLatitudeText(value) { latitudeText = value }
                function setLongitudeText(value) { longitudeText = value }
                function setElevationText(value) {
                    lastElevationText = value
                    elevationText = value
                }
                function refreshCurrentLocation() { ++refreshCurrentLocationCount }
                function setProjectionTypeText(value) { projectionTypeText = value }
                function setThemeId(value) { themeId = value }
                function setLogToTerminal(value) { logToTerminal = value }
                function setLogToFile(value) { logToFile = value }
                function setLogFilePath(value) { logFilePath = value }
                function setCatalogPresetIndex(value) { lastCatalogPresetIndex = value }
                function setCatalogUrlText(value) { lastCatalogUrlText = value }
                function setDeepSkyCatalogPresetIndex(value) {
                    lastDeepSkyCatalogPresetIndex = value
                }
                function setDeepSkyCatalogUrlText(value) { lastDeepSkyCatalogUrlText = value }
            }
            PreferencesDraft {
                id: draft
                skyContextController: fakeController
            }
        }
    )"), QStringLiteral("PreferencesDraftCurrentDeviceBehaviorTest.qml"));
    QVERIFY(object != nullptr);
    QObject* draft = qvariant_cast<QObject*>(object->property("draft"));
    QObject* fakeController = qvariant_cast<QObject*>(object->property("fakeController"));
    QVERIFY(draft != nullptr);
    QVERIFY(fakeController != nullptr);

    QVERIFY(QMetaObject::invokeMethod(draft, "resetFromContext"));
    draft->setProperty("locationSourceText", QStringLiteral("Current Device"));
    draft->setProperty("elevationText", QStringLiteral("123.0"));
    QVERIFY(QMetaObject::invokeMethod(draft, "applyToContext"));
    QCOMPARE(
        fakeController->property("lastLocationSourceText").toString(),
        QString("Current Device")
    );
    QCOMPARE(fakeController->property("lastElevationText").toString(), QString("123.0"));
    QCOMPARE(fakeController->property("refreshCurrentLocationCount").toInt(), 1);

    fakeController->setProperty("latitudeText", QStringLiteral("12.500000"));
    fakeController->setProperty("longitudeText", QStringLiteral("34.500000"));
    fakeController->setProperty("elevationText", QStringLiteral("88.0"));
    QVERIFY(QMetaObject::invokeMethod(draft, "syncDeviceLocationFromContext"));
    QCOMPARE(draft->property("latitudeText").toString(), QString("12.500000"));
    QCOMPARE(draft->property("longitudeText").toString(), QString("34.500000"));
    QCOMPARE(draft->property("elevationText").toString(), QString("88.0"));

    draft->setProperty("locationSourceText", QStringLiteral("Custom"));
    fakeController->setProperty("latitudeText", QStringLiteral("55.000000"));
    QVERIFY(QMetaObject::invokeMethod(draft, "syncDeviceLocationFromContext"));
    QCOMPARE(draft->property("latitudeText").toString(), QString("12.500000"));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesTests::preferencesWindowShellNavigatesAppliesAndCloses()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    controller->setLatitudeText(QStringLiteral("47.000000"));
    controller->setLongitudeText(QStringLiteral("8.000000"));
    controller->setElevationText(QStringLiteral("400"));

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createFileComponent(
        engine,
        QStringLiteral("PreferencesWindow.qml"),
        {{QStringLiteral("skyContextController"), QVariant::fromValue<QObject*>(controller.get())}}
    );
    QVERIFY(object != nullptr);
    auto* window = qobject_cast<QQuickWindow*>(object.get());
    QVERIFY(window != nullptr);

    QObject* draft = firstObjectWithMetaProperties(
        window,
        {"overlayHorizon", "catalogPresetIndex", "deepSkyCatalogPresetIndex"}
    );
    QVERIFY(draft != nullptr);
    draft->setProperty("latitudeText", QStringLiteral("1.000000"));

    QTest::ignoreMessage(QtWarningMsg, "This plugin does not support raise()");
    QVERIFY(QMetaObject::invokeMethod(window, "openWindow"));
    QTRY_VERIFY(window->isVisible());
    QCOMPARE(draft->property("latitudeText").toString(), QString("47.000000"));

    QObject* catalogSection = firstObjectWithProperty(window, "label", QStringLiteral("Catalog"));
    QVERIFY(catalogSection != nullptr);
    QVERIFY(activateControl(catalogSection));
    QTRY_COMPARE(window->property("selectedPage").toInt(), 3);
    QCOMPARE(
        window->property("currentSectionDescription").toString(),
        QString("Catalog source and download settings")
    );

    QObject* skySection = firstObjectWithProperty(window, "label", QStringLiteral("Sky"));
    QVERIFY(skySection != nullptr);
    QVERIFY(activateControl(skySection));
    QTRY_COMPARE(window->property("selectedPage").toInt(), 1);

    const auto applyButtons = invokableButtonsWithText(window, QStringLiteral("Apply"));
    QCOMPARE(applyButtons.size(), 1);
    QObject* applyButton = applyButtons.front();
    draft->setProperty("locationSourceText", QStringLiteral("City"));
    draft->setProperty("selectedCityId", QString());
    QCoreApplication::processEvents();
    QTRY_VERIFY(!window->property("applyEnabled").toBool());
    QVERIFY(!applyButton->property("enabled").toBool());

    draft->setProperty("locationSourceText", QStringLiteral("Custom"));
    draft->setProperty("latitudeText", QStringLiteral("12.500000"));
    draft->setProperty("longitudeText", QStringLiteral("34.500000"));
    draft->setProperty("elevationText", QStringLiteral("88"));
    QCoreApplication::processEvents();
    QTRY_VERIFY(window->property("applyEnabled").toBool());
    QVERIFY(applyButton->property("enabled").toBool());
    QVERIFY(activateControl(applyButton));
    QTRY_COMPARE(controller->latitudeText(), QString("12.500000"));
    QCOMPARE(controller->longitudeText(), QString("34.500000"));
    QCOMPARE(controller->elevationText(), QString("88.0"));

    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(!window->isVisible());
    QStringList unexpectedWarnings = warnings.messages();
    unexpectedWarnings.removeAll(QStringLiteral("This plugin does not support raise()"));
    QVERIFY2(unexpectedWarnings.isEmpty(), qPrintable(unexpectedWarnings.join('\n')));
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
        QStringLiteral("PreferencesDraft.qml"),
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
    object->setProperty("timeZoneId", QStringLiteral("Asia/Bishkek"));
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
    QCOMPARE(restoredController->timeController()->timeZoneId(), QString("Asia/Bishkek"));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

SKYGATE_QML_TEST_MAIN(QmlPreferencesTests)

#include "QmlPreferencesTests.moc"
