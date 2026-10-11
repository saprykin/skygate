#include "QmlPreferencesTestSupport.hpp"
#include "SkyCatalogSourceCollectionModel.hpp"

#include <QDir>
#include <QSettings>

namespace {

skygate::ui::internal::SkyCatalogSourceCollectionModel* sourceCollectionModel(SkyContextController& controller)
{
    return qobject_cast<skygate::ui::internal::SkyCatalogSourceCollectionModel*>(
        controller.catalogSourceCollectionModel()
    );
}

QStringList sourceModelInstanceIds(SkyContextController& controller)
{
    skygate::ui::internal::SkyCatalogSourceCollectionModel* model = sourceCollectionModel(controller);
    if (model == nullptr) {
        return {};
    }

    QStringList instanceIds;
    for (int row = 0; row < model->rowCount(); ++row) {
        instanceIds.push_back(
            model->data(model->index(row, 0), skygate::ui::internal::SkyCatalogSourceCollectionModel::InstanceIdRole)
                .toString()
        );
    }
    return instanceIds;
}

QString sourceModelRowStatus(SkyContextController& controller, const int row)
{
    skygate::ui::internal::SkyCatalogSourceCollectionModel* model = sourceCollectionModel(controller);
    if (model == nullptr || row < 0 || row >= model->rowCount()) {
        return {};
    }

    return model->data(model->index(row, 0), skygate::ui::internal::SkyCatalogSourceCollectionModel::StatusRole)
        .toString();
}

int sourceModelRowWithError(SkyContextController& controller)
{
    skygate::ui::internal::SkyCatalogSourceCollectionModel* model = sourceCollectionModel(controller);
    if (model == nullptr) {
        return -1;
    }

    for (int row = 0; row < model->rowCount(); ++row) {
        const bool hasError =
            model->data(model->index(row, 0), skygate::ui::internal::SkyCatalogSourceCollectionModel::HasErrorRole)
                .toBool();
        if (hasError) {
            return row;
        }
    }
    return -1;
}

void addCustomCatalogSource(
    QQuickItem* root,
    QQuickWindow* window,
    SkyContextController& controller,
    QObject* presetCombo,
    const int comboIndex,
    const QString& url
)
{
    presetCombo->setProperty("currentIndex", comboIndex);
    QCoreApplication::processEvents();

    QQuickItem* urlInput = firstQuickItemWithObjectName(root, QStringLiteral("catalogSourceUrlInput"));
    QVERIFY(urlInput != nullptr);
    QTRY_VERIFY(urlInput->isVisible());
    replaceText(window, urlInput, url);

    QObject* addButton = firstObjectWithObjectName(root, QStringLiteral("catalogAddSourceButton"));
    QVERIFY(addButton != nullptr);
    QTRY_VERIFY(addButton->property("enabled").toBool());
    QVERIFY(activateControl(addButton));
    QTRY_VERIFY(!controller.downloadingCatalog() && !controller.catalogProcessing());
}

}  // namespace

class QmlPreferencesCatalogTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void catalogSectionShowsPresetsAndAddsSources();
    void catalogSectionManagesSourcesOrderingAndRemoval();
    void catalogSectionReportsErrorsAndRetries();
    void catalogSectionRestoresCollectionAndIdentity();
    void catalogSectionPresentsFallbackParticipation();
    void catalogSectionPresentsSourceRowsAndSummary();

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
    QSettings settings;
    settings.setValue(
        QStringLiteral("skyContext/catalogCollectionCachePath"),
        m_settings.cachePath(QStringLiteral("catalog-collection-cache"))
    );
    QDir(m_settings.cachePath(QStringLiteral("catalog-collection-cache"))).removeRecursively();
}

void QmlPreferencesCatalogTests::catalogSectionShowsPresetsAndAddsSources()
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
            PreferencesCatalogSection {
                anchors.fill: parent
                skyContextController: skyContext
            }
        }
    )"),
        QStringLiteral("PreferencesCatalogSectionSourcesTest.qml")
    );
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    ExposedQuickWindow exposed(root);

    // The preset picker is data-driven: two preset descriptors plus two
    // generic custom rows with an explicit category. Adding another preset
    // therefore only requires a descriptor entry, not another QML branch.
    QObject* presetCombo = firstObjectWithObjectName(root, QStringLiteral("catalogSourcePresetCombo"));
    QVERIFY(presetCombo != nullptr);
    QCOMPARE(presetCombo->property("count").toInt(), 4);
    QCOMPARE(controller->catalogSourcePresetId(0), QStringLiteral("hyg_v42"));
    QCOMPARE(controller->catalogSourcePresetId(1), QStringLiteral("open_ngc"));
    QVERIFY(!controller->catalogSourcePresetIsCustom(0));
    QVERIFY(!controller->catalogSourcePresetIsCustom(1));
    QVERIFY(controller->catalogSourcePresetIsCustom(2));
    QVERIFY(controller->catalogSourcePresetIsCustom(3));
    QCOMPARE(controller->catalogSourcePresetCategory(0), QStringLiteral("Star"));
    QCOMPARE(controller->catalogSourcePresetCategory(1), QStringLiteral("Deep sky"));
    QCOMPARE(controller->catalogSourcePresetCategory(2), QStringLiteral("Star"));
    QCOMPARE(controller->catalogSourcePresetCategory(3), QStringLiteral("Deep sky"));

    // The default bundled source is present, enabled, and not removable.
    skygate::ui::internal::SkyCatalogSourceCollectionModel* model = sourceCollectionModel(*controller);
    QVERIFY(model != nullptr);
    QTRY_COMPARE(model->rowCount(), 1);
    QCOMPARE(
        model->data(model->index(0, 0), skygate::ui::internal::SkyCatalogSourceCollectionModel::BundledRole).toBool(),
        true
    );

    const QString starPath = m_settings.cachePath(QStringLiteral("add-source-star.csv"));
    QVERIFY(writeFile(
        starPath,
        sampleHygCsvPayload(
            {.id = 1, .hip = 900101, .properName = "Added Star", .ra = "6.0", .dec = "-16.0", .mag = "1.0"}
        )
    ));
    const QString starUrl = QUrl::fromLocalFile(starPath).toString();
    addCustomCatalogSource(root, exposed.window(), *controller, presetCombo, 2, starUrl);

    QTRY_VERIFY(catalogContainsDisplayName(controller->catalogBodies(), QStringLiteral("Added Star")));
    QTRY_COMPARE(model->rowCount(), 2);
    QCOMPARE(sourceModelInstanceIds(*controller).size(), 2);
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesCatalogTests::catalogSectionManagesSourcesOrderingAndRemoval()
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
            height: 720
            PreferencesCatalogSection {
                anchors.fill: parent
                skyContextController: skyContext
            }
        }
    )"),
        QStringLiteral("PreferencesCatalogSectionOrderingTest.qml")
    );
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    ExposedQuickWindow exposed(root);

    QObject* presetCombo = firstObjectWithObjectName(root, QStringLiteral("catalogSourcePresetCombo"));
    QVERIFY(presetCombo != nullptr);

    const QString starAPath = m_settings.cachePath(QStringLiteral("order-star-a.csv"));
    const QString starBPath = m_settings.cachePath(QStringLiteral("order-star-b.csv"));
    const QString deepSkyPath = m_settings.cachePath(QStringLiteral("order-dso.csv"));
    QVERIFY(writeFile(
        starAPath,
        sampleHygCsvPayload(
            {.id = 1, .hip = 900201, .properName = "Ordered Star A", .ra = "6.0", .dec = "-16.0", .mag = "1.0"}
        )
    ));
    QVERIFY(writeFile(
        starBPath,
        sampleHygCsvPayload(
            {.id = 2, .hip = 900202, .properName = "Ordered Star B", .ra = "6.1", .dec = "-16.1", .mag = "2.0"}
        )
    ));
    QVERIFY(writeFile(
        deepSkyPath,
        sampleOpenNgcCsvPayload(
            {.name = "NGC0999",
             .type = "G",
             .ra = "00:42:44.35",
             .dec = "+41:16:08.6",
             .messier = "",
             .ngc = "0999",
             .identifiers = "PGC 9999",
             .commonName = "Ordered Galaxy C"}
        )
    ));

    addCustomCatalogSource(
        root, exposed.window(), *controller, presetCombo, 2, QUrl::fromLocalFile(starAPath).toString()
    );
    addCustomCatalogSource(
        root, exposed.window(), *controller, presetCombo, 2, QUrl::fromLocalFile(starBPath).toString()
    );
    addCustomCatalogSource(
        root, exposed.window(), *controller, presetCombo, 3, QUrl::fromLocalFile(deepSkyPath).toString()
    );

    skygate::ui::internal::SkyCatalogSourceCollectionModel* model = sourceCollectionModel(*controller);
    QVERIFY(model != nullptr);
    QTRY_COMPARE(model->rowCount(), 4);
    const QStringList addedIds = sourceModelInstanceIds(*controller);
    QCOMPARE(addedIds.size(), 4);
    const QString bundledId = addedIds[0];
    const QString starAId = addedIds[1];
    const QString starBId = addedIds[2];
    const QString deepSkyId = addedIds[3];
    QVERIFY(starAId != starBId);
    QVERIFY(starBId != deepSkyId);
    QVERIFY(catalogContainsDisplayName(controller->catalogBodies(), QStringLiteral("Ordered Star A")));
    QVERIFY(catalogContainsDisplayName(controller->catalogBodies(), QStringLiteral("Ordered Star B")));

    // Move the deep-sky source up: [bundled, A, C, B].
    QObject* upButton = firstObjectWithObjectName(root, QStringLiteral("catalogSourceUpButton_3"));
    QVERIFY(upButton != nullptr);
    QVERIFY(activateControl(upButton));
    QTRY_COMPARE(sourceModelInstanceIds(*controller), QStringList({bundledId, starAId, deepSkyId, starBId}));

    // Disabling A removes only its contribution.
    QObject* enableCheckBox = firstObjectWithObjectName(root, QStringLiteral("catalogSourceEnableCheckBox_1"));
    QVERIFY(enableCheckBox != nullptr);
    QVERIFY(activateControl(enableCheckBox));
    QTRY_VERIFY(!catalogContainsDisplayName(controller->catalogBodies(), QStringLiteral("Ordered Star A")));
    QVERIFY(catalogContainsDisplayName(controller->catalogBodies(), QStringLiteral("Ordered Star B")));

    enableCheckBox = firstObjectWithObjectName(root, QStringLiteral("catalogSourceEnableCheckBox_1"));
    QVERIFY(enableCheckBox != nullptr);
    QVERIFY(activateControl(enableCheckBox));
    QTRY_VERIFY(catalogContainsDisplayName(controller->catalogBodies(), QStringLiteral("Ordered Star A")));

    // Removing A leaves the other two sources intact.
    QObject* removeButton = firstObjectWithObjectName(root, QStringLiteral("catalogSourceRemoveButton_1"));
    QVERIFY(removeButton != nullptr);
    QVERIFY(activateControl(removeButton));
    QTRY_COMPARE(sourceModelInstanceIds(*controller), QStringList({bundledId, deepSkyId, starBId}));
    QTRY_VERIFY(!catalogContainsDisplayName(controller->catalogBodies(), QStringLiteral("Ordered Star A")));
    QVERIFY(catalogContainsDisplayName(controller->catalogBodies(), QStringLiteral("Ordered Star B")));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesCatalogTests::catalogSectionReportsErrorsAndRetries()
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
            PreferencesCatalogSection {
                anchors.fill: parent
                skyContextController: skyContext
            }
        }
    )"),
        QStringLiteral("PreferencesCatalogSectionErrorTest.qml")
    );
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    ExposedQuickWindow exposed(root);

    QObject* presetCombo = firstObjectWithObjectName(root, QStringLiteral("catalogSourcePresetCombo"));
    QVERIFY(presetCombo != nullptr);

    const QString retryPath = m_settings.cachePath(QStringLiteral("retry-star.csv"));
    const QString retryUrl = QUrl::fromLocalFile(retryPath).toString();
    addCustomCatalogSource(root, exposed.window(), *controller, presetCombo, 2, retryUrl);

    skygate::ui::internal::SkyCatalogSourceCollectionModel* model = sourceCollectionModel(*controller);
    QVERIFY(model != nullptr);
    QTRY_COMPARE(model->rowCount(), 2);
    const int errorRow = sourceModelRowWithError(*controller);
    QVERIFY(errorRow >= 0);

    QObject* retryButton =
        firstObjectWithObjectName(root, QStringLiteral("catalogSourceRetryButton_") + QString::number(errorRow));
    QVERIFY(retryButton != nullptr);
    QVERIFY(retryButton->property("visible").toBool());

    QVERIFY(writeFile(
        retryPath,
        sampleHygCsvPayload(
            {.id = 1, .hip = 900301, .properName = "Retried Star", .ra = "6.0", .dec = "-16.0", .mag = "1.0"}
        )
    ));
    QVERIFY(activateControl(retryButton));
    QTRY_VERIFY(!controller->downloadingCatalog() && !controller->catalogProcessing());
    QTRY_VERIFY(catalogContainsDisplayName(controller->catalogBodies(), QStringLiteral("Retried Star")));
    QTRY_VERIFY(sourceModelRowWithError(*controller) < 0);

    QStringList unexpectedWarnings;
    for (const QString& message : warnings.messages()) {
        if (!message.startsWith(QStringLiteral("Catalog source failed"))) {
            unexpectedWarnings.push_back(message);
        }
    }
    QVERIFY2(unexpectedWarnings.isEmpty(), qPrintable(unexpectedWarnings.join('\n')));
}

void QmlPreferencesCatalogTests::catalogSectionRestoresCollectionAndIdentity()
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
            PreferencesCatalogSection {
                anchors.fill: parent
                skyContextController: skyContext
            }
        }
    )"),
        QStringLiteral("PreferencesCatalogSectionRestoreTest.qml")
    );
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    ExposedQuickWindow exposed(root);

    QObject* presetCombo = firstObjectWithObjectName(root, QStringLiteral("catalogSourcePresetCombo"));
    QVERIFY(presetCombo != nullptr);

    const QString starAPath = m_settings.cachePath(QStringLiteral("restore-star-a.csv"));
    const QString starBPath = m_settings.cachePath(QStringLiteral("restore-star-b.csv"));
    const QString deepSkyPath = m_settings.cachePath(QStringLiteral("restore-dso.csv"));
    QVERIFY(writeFile(
        starAPath,
        sampleHygCsvPayload(
            {.id = 1, .hip = 900401, .properName = "Restored Star A", .ra = "6.0", .dec = "-16.0", .mag = "1.0"}
        )
    ));
    QVERIFY(writeFile(
        starBPath,
        sampleHygCsvPayload(
            {.id = 2, .hip = 900402, .properName = "Restored Star B", .ra = "6.1", .dec = "-16.1", .mag = "2.0"}
        )
    ));
    QVERIFY(writeFile(
        deepSkyPath,
        sampleOpenNgcCsvPayload(
            {.name = "NGC0998",
             .type = "G",
             .ra = "00:42:44.35",
             .dec = "+41:16:08.6",
             .messier = "",
             .ngc = "0998",
             .identifiers = "PGC 9998",
             .commonName = "Restored Galaxy C"}
        )
    ));

    const QString starAUrl = QUrl::fromLocalFile(starAPath).toString();
    const QString starBUrl = QUrl::fromLocalFile(starBPath).toString();
    const QString deepSkyUrl = QUrl::fromLocalFile(deepSkyPath).toString();
    addCustomCatalogSource(root, exposed.window(), *controller, presetCombo, 2, starAUrl);
    addCustomCatalogSource(root, exposed.window(), *controller, presetCombo, 2, starBUrl);
    addCustomCatalogSource(root, exposed.window(), *controller, presetCombo, 3, deepSkyUrl);

    QTRY_COMPARE(sourceModelInstanceIds(*controller).size(), 4);
    const QStringList addedIds = sourceModelInstanceIds(*controller);
    const QString bundledId = addedIds[0];
    const QString starAId = addedIds[1];
    const QString starBId = addedIds[2];
    const QString deepSkyId = addedIds[3];
    QVERIFY(starAId != starBId);
    QVERIFY(starBId != deepSkyId);
    QVERIFY(sourceModelInstanceIds(*controller).contains(starAId));
    QVERIFY(sourceModelInstanceIds(*controller).contains(starBId));
    QVERIFY(sourceModelInstanceIds(*controller).contains(deepSkyId));

    QVERIFY(controller->saveSettings());
    auto restoredController = makeController();
    QVERIFY(restoredController != nullptr);
    QTRY_VERIFY(catalogContainsDisplayName(restoredController->catalogBodies(), QStringLiteral("Restored Star A")));
    QTRY_VERIFY(catalogContainsDisplayName(restoredController->catalogBodies(), QStringLiteral("Restored Star B")));

    skygate::ui::internal::SkyCatalogSourceCollectionModel* restoredModel = sourceCollectionModel(*restoredController);
    QVERIFY(restoredModel != nullptr);
    // The bundled source is restored with its position and participation next
    // to the three downloaded sources.
    QTRY_COMPARE(restoredModel->rowCount(), 4);
    const QStringList restoredIds = sourceModelInstanceIds(*restoredController);
    QCOMPARE(restoredIds[0], bundledId);
    QVERIFY(restoredIds.contains(starAId));
    QVERIFY(restoredIds.contains(starBId));
    QVERIFY(restoredIds.contains(deepSkyId));

    // A restored title gains a "(saved)" suffix, but the source identity is
    // unchanged, so QML bindings keyed on the model identity remain stable.
    const int restoredRow = restoredModel->indexOfInstanceId(starAId);
    QVERIFY(restoredRow >= 0);
    const QString restoredTitle =
        restoredModel
            ->data(
                restoredModel->index(restoredRow, 0), skygate::ui::internal::SkyCatalogSourceCollectionModel::TitleRole
            )
            .toString();
    QVERIFY(!restoredTitle.isEmpty());
    QVERIFY(restoredTitle.contains(QStringLiteral("saved")));
    QCOMPARE(
        restoredModel
            ->data(
                restoredModel->index(restoredRow, 0),
                skygate::ui::internal::SkyCatalogSourceCollectionModel::InstanceIdRole
            )
            .toString(),
        starAId
    );
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesCatalogTests::catalogSectionPresentsFallbackParticipation()
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
            PreferencesCatalogSection {
                anchors.fill: parent
                skyContextController: skyContext
            }
        }
    )"),
        QStringLiteral("PreferencesCatalogSectionFallbackTest.qml")
    );
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    ExposedQuickWindow exposed(root);

    QQuickItem* summaryLabel = firstQuickItemWithObjectName(root, QStringLiteral("catalogParticipationSummaryLabel"));
    QVERIFY(summaryLabel != nullptr);

    // The bundled star source supplies the bundled deep-sky objects, so the
    // enabled fallback is not presented as the active deep-sky source.
    QTRY_COMPARE(summaryLabel->property("text").toString(), QStringLiteral("Bundled + Bundled core"));
    QVERIFY(!controller->catalogStatusText().contains(QStringLiteral("Bundled Messier")));

    // Replacing the bundled star source with a star-only catalog lets the
    // fallback supply the deep-sky objects, and the section presents it.
    const QString starPath = m_settings.cachePath(QStringLiteral("fallback-presentation-star.csv"));
    QVERIFY(writeFile(
        starPath,
        sampleHygCsvPayload(
            {.id = 1,
             .hip = 900501,
             .properName = "Fallback Presentation Star",
             .ra = "6.0",
             .dec = "-16.0",
             .mag = "1.0"}
        )
    ));
    controller->downloadCatalogFromUrl(QUrl::fromLocalFile(starPath).toString());
    QTRY_VERIFY(!controller->downloadingCatalog() && !controller->catalogProcessing());
    QTRY_VERIFY(summaryLabel->property("text").toString().startsWith(QStringLiteral("Downloaded")));
    QTRY_VERIFY(summaryLabel->property("text").toString().contains(QStringLiteral("Bundled Messier")));
    QVERIFY(controller->catalogStatusText().contains(QStringLiteral("Bundled Messier")));

    // Reloading the bundled star source removes the fallback's participation
    // again, so the summary never keeps a stale bundled claim.
    controller->loadCatalogPreset(QStringLiteral("bundled"));
    QTRY_COMPARE(summaryLabel->property("text").toString(), QStringLiteral("Bundled + Bundled core"));
    QVERIFY(!controller->catalogStatusText().contains(QStringLiteral("Bundled Messier")));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlPreferencesCatalogTests::catalogSectionPresentsSourceRowsAndSummary()
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
            PreferencesCatalogSection {
                anchors.fill: parent
                skyContextController: skyContext
            }
        }
    )"),
        QStringLiteral("PreferencesCatalogSectionParticipationTest.qml")
    );
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);
    ExposedQuickWindow exposed(root);

    QObject* presetCombo = firstObjectWithObjectName(root, QStringLiteral("catalogSourcePresetCombo"));
    QVERIFY(presetCombo != nullptr);
    QQuickItem* summaryLabel = firstQuickItemWithObjectName(root, QStringLiteral("catalogParticipationSummaryLabel"));
    QVERIFY(summaryLabel != nullptr);

    skygate::ui::internal::SkyCatalogSourceCollectionModel* model = sourceCollectionModel(*controller);
    QVERIFY(model != nullptr);
    QTRY_COMPARE(model->rowCount(), 1);

    const QString starPath = m_settings.cachePath(QStringLiteral("participation-presentation-star.csv"));
    QVERIFY(writeFile(
        starPath,
        sampleHygCsvPayload(
            {.id = 2, .hip = 900502, .properName = "Participation Star", .ra = "6.1", .dec = "-16.1", .mag = "2.0"}
        )
    ));
    addCustomCatalogSource(
        root, exposed.window(), *controller, presetCombo, 2, QUrl::fromLocalFile(starPath).toString()
    );

    // The added source is named by the summary and its own row reports the
    // active state.
    QTRY_COMPARE(model->rowCount(), 2);
    QTRY_COMPARE(summaryLabel->property("text").toString(), QStringLiteral("Bundled + Downloaded"));
    QCOMPARE(sourceModelRowStatus(*controller, 1), QStringLiteral("Active"));
    QCOMPARE(sourceModelRowStatus(*controller, 0), QStringLiteral("Active"));

    // Disabling the source removes it from the summary and marks only its row
    // as disabled; the bundled participation fills the gap again.
    QObject* enableCheckBox = firstObjectWithObjectName(root, QStringLiteral("catalogSourceEnableCheckBox_1"));
    QVERIFY(enableCheckBox != nullptr);
    QVERIFY(activateControl(enableCheckBox));
    QTRY_COMPARE(sourceModelRowStatus(*controller, 1), QStringLiteral("Disabled"));
    QTRY_COMPARE(summaryLabel->property("text").toString(), QStringLiteral("Bundled + Bundled core"));
    QVERIFY(!controller->catalogStatusText().contains(QStringLiteral("Downloaded")));
    QCOMPARE(sourceModelRowStatus(*controller, 0), QStringLiteral("Active"));
    QTRY_VERIFY(!catalogContainsDisplayName(controller->catalogBodies(), QStringLiteral("Participation Star")));

    // Re-enabling restores the source's participation and its row state.
    enableCheckBox = firstObjectWithObjectName(root, QStringLiteral("catalogSourceEnableCheckBox_1"));
    QVERIFY(enableCheckBox != nullptr);
    QVERIFY(activateControl(enableCheckBox));
    QTRY_COMPARE(summaryLabel->property("text").toString(), QStringLiteral("Bundled + Downloaded"));
    QCOMPARE(sourceModelRowStatus(*controller, 1), QStringLiteral("Active"));

    // Removing the source drops its row and its title from the summary.
    QObject* removeButton = firstObjectWithObjectName(root, QStringLiteral("catalogSourceRemoveButton_1"));
    QVERIFY(removeButton != nullptr);
    QVERIFY(activateControl(removeButton));
    QTRY_COMPARE(model->rowCount(), 1);
    QTRY_COMPARE(summaryLabel->property("text").toString(), QStringLiteral("Bundled + Bundled core"));
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

SKYGATE_QML_TEST_MAIN(QmlPreferencesCatalogTests)

#include "QmlPreferencesCatalogTests.moc"
