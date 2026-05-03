#include "QmlTestSupport.hpp"

using namespace skygate::ui::tests;

namespace {

QQuickItem* searchResultDelegate(QObject* root, const QString& targetId)
{
    for (QObject* object : objectTree(root)) {
        auto* item = qobject_cast<QQuickItem*>(object);
        if (
            item != nullptr
            && item->isVisible()
            && object->property("targetId").toString() == targetId
            && object->metaObject()->indexOfMethod("activate()") >= 0
        ) {
            return item;
        }
    }
    return nullptr;
}

}  // namespace

class QmlToolbarTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void searchToolbarFiltersActivatesAndClearsResults();
    void searchToolbarToggleRequestsExpandAndClearsSearch();
    void searchToolbarDelegateClickEmptyStateAndTrackingClearWork();
    void timelineToolbarControlsUpdateController();
    void timelineToolbarToggleResetAndContextSyncWork();

private:
    QmlSettingsFixture m_settings;
};

void QmlToolbarTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("QmlToolbarTests")));
}

void QmlToolbarTests::init()
{
    m_settings.resetForCurrentTest();
}

void QmlToolbarTests::searchToolbarFiltersActivatesAndClearsResults()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    controller->setSearchToolbarCollapsed(false);
    auto* searchModel = qobject_cast<SkyObjectSearchModel*>(controller->objectSearchModel());
    QVERIFY(searchModel != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createFileComponent(
        engine,
        QStringLiteral("SearchToolbar.qml"),
        {{QStringLiteral("skyContextController"), QVariant::fromValue<QObject*>(controller.get())}}
    );
    QVERIFY(object != nullptr);
    auto* toolbar = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(toolbar != nullptr);

    ExposedQuickWindow exposed(toolbar);
    auto* searchField = qobject_cast<QQuickItem*>(firstObjectWithProperty(
        toolbar,
        "placeholderText",
        QStringLiteral("Search planets, stars, HIP, constellations")
    ));
    QVERIFY(searchField != nullptr);

    QVERIFY(QMetaObject::invokeMethod(searchField, "forceActiveFocus"));
    commitText(exposed.window(), QStringLiteral("Sirius"));
    QTRY_COMPARE(searchModel->filterText(), QString("Sirius"));
    QVERIFY(searchModel->rowCount() > 0);

    auto* dropdown = quickItemProperty(toolbar, "dropdownItem");
    QVERIFY(dropdown != nullptr);
    QTRY_VERIFY(dropdown->isVisible());

    const QModelIndex firstResult = searchModel->index(0, 0);
    const QString targetId = searchModel->data(
        firstResult,
        SkyObjectSearchModel::TargetIdRole
    ).toString();
    QVERIFY(!targetId.isEmpty());

    QTest::keyClick(exposed.window(), Qt::Key_Return);
    QTRY_COMPARE(controller->selectedSearchTargetId(), targetId);

    const auto clearButtons = invokableButtonsWithText(
        toolbar,
        QString::fromUtf8("\xE2\x9C\x95")
    );
    QVERIFY(!clearButtons.empty());
    QVERIFY(clearButtons.front()->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(searchField, "forceActiveFocus"));
    QTest::keyClick(exposed.window(), Qt::Key_Escape);
    QTRY_COMPARE(searchField->property("text").toString(), QString());
    QCOMPARE(searchModel->filterText(), QString());
    QVERIFY(controller->selectedSearchTargetId().isEmpty());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlToolbarTests::searchToolbarToggleRequestsExpandAndClearsSearch()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    controller->setSearchToolbarCollapsed(false);
    auto* searchModel = qobject_cast<SkyObjectSearchModel*>(controller->objectSearchModel());
    QVERIFY(searchModel != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createInlineComponent(engine, QStringLiteral(R"(
        import QtQuick
        Item {
            id: root
            width: 900
            height: 120
            property int requestExpandCount: 0
            SearchToolbar {
                id: searchToolbar
                anchors.fill: parent
                skyContextController: skyContext
                onRequestExpand: function() { ++root.requestExpandCount }
            }
        }
    )"), QStringLiteral("SearchToolbarToggleBehaviorTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    ExposedQuickWindow exposed(root);
    auto* searchField = qobject_cast<QQuickItem*>(firstObjectWithProperty(
        root,
        "placeholderText",
        QStringLiteral("Search planets, stars, HIP, constellations")
    ));
    QVERIFY(searchField != nullptr);
    QVERIFY(QMetaObject::invokeMethod(searchField, "forceActiveFocus"));
    commitText(exposed.window(), QStringLiteral("Vega"));
    QTRY_COMPARE(searchModel->filterText(), QString("Vega"));

    const auto toggles = invokableButtonsWithText(root, QString::fromUtf8("\xE2\x96\xB6"));
    QCOMPARE(toggles.size(), 1);
    QVERIFY(activateControl(toggles.front()));
    QTRY_VERIFY(controller->searchToolbarCollapsed());
    QTRY_COMPARE(searchField->property("text").toString(), QString());
    QCOMPARE(searchModel->filterText(), QString());

    const auto collapsedToggles = invokableButtonsWithText(
        root,
        QString::fromUtf8("\xE2\x97\x80")
    );
    QCOMPARE(collapsedToggles.size(), 1);
    QVERIFY(activateControl(collapsedToggles.front()));
    QTRY_VERIFY(!controller->searchToolbarCollapsed());
    QCOMPARE(root->property("requestExpandCount").toInt(), 1);
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlToolbarTests::searchToolbarDelegateClickEmptyStateAndTrackingClearWork()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    controller->setSearchToolbarCollapsed(false);
    QVERIFY(controller->trackSearchTarget(QStringLiteral("body"), QStringLiteral("sun")));
    QVERIFY(controller->hasTrackedTarget());
    auto* searchModel = qobject_cast<SkyObjectSearchModel*>(controller->objectSearchModel());
    QVERIFY(searchModel != nullptr);

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createInlineComponent(engine, QStringLiteral(R"(
        import QtQuick
        Item {
            width: 900
            height: 180
            SearchToolbar {
                id: searchToolbar
                anchors.fill: parent
                skyContextController: skyContext
            }
        }
    )"), QStringLiteral("SearchToolbarDelegateBehaviorTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    ExposedQuickWindow exposed(root);
    auto* searchField = qobject_cast<QQuickItem*>(firstObjectWithProperty(
        root,
        "placeholderText",
        QStringLiteral("Search planets, stars, HIP, constellations")
    ));
    QVERIFY(searchField != nullptr);

    QVERIFY(QMetaObject::invokeMethod(searchField, "forceActiveFocus"));
    commitText(exposed.window(), QStringLiteral("definitely-no-sky-object"));
    QTRY_COMPARE(searchModel->filterText(), QString("definitely-no-sky-object"));
    QTRY_COMPARE(searchModel->rowCount(), 0);
    QTRY_VERIFY(firstVisibleItemWithText(root, QStringLiteral("No matching objects")) != nullptr);

    replaceText(exposed.window(), searchField, QStringLiteral("Sirius"));
    QTRY_VERIFY(searchModel->rowCount() > 0);
    const QModelIndex firstResult = searchModel->index(0, 0);
    const QString displayText = searchModel->data(
        firstResult,
        SkyObjectSearchModel::DisplayTextRole
    ).toString();
    const QString targetId = searchModel->data(
        firstResult,
        SkyObjectSearchModel::TargetIdRole
    ).toString();
    QVERIFY(!displayText.isEmpty());
    QVERIFY(!targetId.isEmpty());

    auto* resultDelegate = searchResultDelegate(root, targetId);
    QVERIFY(resultDelegate != nullptr);
    const QPoint clickPoint = resultDelegate->mapToScene(
        QPointF(resultDelegate->width() * 0.5, resultDelegate->height() * 0.5)
    ).toPoint();
    QTest::mouseClick(exposed.window(), Qt::LeftButton, Qt::NoModifier, clickPoint);
    QTRY_COMPARE(controller->selectedSearchTargetId(), targetId);
    QCOMPARE(controller->selectedSearchTargetKind(), QString("body"));
    QVERIFY(!controller->hasTrackedTarget());
    QCOMPARE(searchModel->filterText(), QString());
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlToolbarTests::timelineToolbarControlsUpdateController()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    controller->setTimelineToolbarCollapsed(false);
    controller->setStepSeconds(60);
    QVERIFY(controller->setUtcDateTimeText("2024-01-02", "03:04:05"));
    controller->setLive(true);
    const QString originalTimeText = controller->utcTimeText();

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createInlineComponent(engine, QStringLiteral(R"(
        import QtQuick
        Item {
            width: 920
            height: 160
            TimelineToolbar {
                id: timelineToolbar
                anchors.top: parent.top
                anchors.right: parent.right
                skyContextController: skyContext
            }
        }
    )"), QStringLiteral("TimelineToolbarBehaviorTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    ExposedQuickWindow exposed(root);
    (void)exposed;

    const auto pauseButtons = invokableButtonsWithText(root, QStringLiteral("Pause"));
    QVERIFY(!pauseButtons.empty());
    QVERIFY(QMetaObject::invokeMethod(pauseButtons.front(), "click"));
    QTRY_VERIFY(!controller->live());

    const auto playButtons = invokableButtonsWithText(root, QStringLiteral("Play"));
    QVERIFY(!playButtons.empty());
    QVERIFY(QMetaObject::invokeMethod(playButtons.front(), "click"));
    QTRY_VERIFY(controller->live());

    const auto stepForwardButtons = invokableButtonsWithText(root, QStringLiteral(">"));
    QVERIFY(!stepForwardButtons.empty());
    QVERIFY(QMetaObject::invokeMethod(stepForwardButtons.front(), "click"));
    QTRY_VERIFY(controller->utcTimeText() != originalTimeText);
    const auto stepBackwardButtons = invokableButtonsWithText(root, QStringLiteral("<"));
    QVERIFY(!stepBackwardButtons.empty());
    QVERIFY(QMetaObject::invokeMethod(stepBackwardButtons.front(), "click"));
    QTRY_COMPARE(controller->utcTimeText(), originalTimeText);

    const auto speedCombos = comboBoxesWithCount(root, 6);
    const auto stepCombos = comboBoxesWithCount(root, 5);
    const auto magnitudeCombos = comboBoxesWithCount(root, 7);
    QCOMPARE(speedCombos.size(), 1);
    QCOMPARE(stepCombos.size(), 1);
    QCOMPARE(magnitudeCombos.size(), 1);

    speedCombos.front()->setProperty("currentIndex", 3);
    QVERIFY(QMetaObject::invokeMethod(speedCombos.front(), "activated", Q_ARG(int, 3)));
    QCOMPARE(controller->speedMultiplier(), 2.0);

    stepCombos.front()->setProperty("currentIndex", 4);
    QVERIFY(QMetaObject::invokeMethod(stepCombos.front(), "activated", Q_ARG(int, 4)));
    QCOMPARE(controller->stepSeconds(), 3600);

    magnitudeCombos.front()->setProperty("currentIndex", 5);
    QVERIFY(QMetaObject::invokeMethod(magnitudeCombos.front(), "activated", Q_ARG(int, 5)));
    QCOMPARE(controller->magnitudeCutoff(), 7.0);
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

void QmlToolbarTests::timelineToolbarToggleResetAndContextSyncWork()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    controller->setTimelineToolbarCollapsed(false);
    controller->setSpeedMultiplier(8.0);
    controller->setStepSeconds(3600);
    controller->setMagnitudeCutoff(8.0);
    controller->setViewCenter(12.0, 123.0);

    QQmlEngine engine;
    setupEngine(engine, *controller);

    const QmlWarningScope warnings;
    auto object = createInlineComponent(engine, QStringLiteral(R"(
        import QtQuick
        Item {
            id: root
            width: 920
            height: 160
            property int requestExpandCount: 0
            TimelineToolbar {
                id: timelineToolbar
                anchors.top: parent.top
                anchors.right: parent.right
                skyContextController: skyContext
                onRequestExpand: function() { ++root.requestExpandCount }
            }
        }
    )"), QStringLiteral("TimelineToolbarToggleBehaviorTest.qml"));
    QVERIFY(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QVERIFY(root != nullptr);

    ExposedQuickWindow exposed(root);
    (void)exposed;

    const auto speedCombos = comboBoxesWithCount(root, 6);
    const auto stepCombos = comboBoxesWithCount(root, 5);
    const auto magnitudeCombos = comboBoxesWithCount(root, 7);
    QCOMPARE(speedCombos.size(), 1);
    QCOMPARE(stepCombos.size(), 1);
    QCOMPARE(magnitudeCombos.size(), 1);
    QTRY_COMPARE(speedCombos.front()->property("currentIndex").toInt(), 5);
    QCOMPARE(stepCombos.front()->property("currentIndex").toInt(), 4);
    QCOMPARE(magnitudeCombos.front()->property("currentIndex").toInt(), 6);

    controller->setSpeedMultiplier(0.5);
    controller->setStepSeconds(10);
    controller->setMagnitudeCutoff(3.0);
    QTRY_COMPARE(speedCombos.front()->property("currentIndex").toInt(), 1);
    QCOMPARE(stepCombos.front()->property("currentIndex").toInt(), 1);
    QCOMPARE(magnitudeCombos.front()->property("currentIndex").toInt(), 1);

    const auto resetButtons = invokableButtonsWithText(root, QStringLiteral("Reset"));
    QCOMPARE(resetButtons.size(), 1);
    QVERIFY(activateControl(resetButtons.front()));
    QTRY_COMPARE(controller->viewCenterAltitudeDeg(), 45.0);
    QCOMPARE(controller->viewCenterAzimuthDeg(), 180.0);

    const auto expandedToggles = invokableButtonsWithText(
        root,
        QString::fromUtf8("\xE2\x97\x80")
    );
    QCOMPARE(expandedToggles.size(), 1);
    QVERIFY(activateControl(expandedToggles.front()));
    QTRY_VERIFY(controller->timelineToolbarCollapsed());

    const auto collapsedToggles = invokableButtonsWithText(root, QString::fromUtf8("\xE2\x96\xB6"));
    QCOMPARE(collapsedToggles.size(), 1);
    QVERIFY(activateControl(collapsedToggles.front()));
    QTRY_VERIFY(!controller->timelineToolbarCollapsed());
    QCOMPARE(root->property("requestExpandCount").toInt(), 1);
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

SKYGATE_QML_TEST_MAIN(QmlToolbarTests)

#include "QmlToolbarTests.moc"
