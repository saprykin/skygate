#include "QmlTestSupport.hpp"

using namespace skygate::ui::tests;

namespace {

QQuickItem* searchResultDelegate(QObject* root, const QString& targetId)
{
    return firstQuickItemWithObjectName(
        root,
        QStringLiteral("searchResultDelegate_%1").arg(targetId)
    );
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
    auto* searchField = firstQuickItemWithObjectName(toolbar, QStringLiteral("searchField"));
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

    QObject* clearButton = firstObjectWithObjectName(
        toolbar,
        QStringLiteral("searchClearButton")
    );
    QVERIFY(clearButton != nullptr);
    QVERIFY(clearButton->property("visible").toBool());
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
    auto* searchField = firstQuickItemWithObjectName(root, QStringLiteral("searchField"));
    QVERIFY(searchField != nullptr);
    QVERIFY(QMetaObject::invokeMethod(searchField, "forceActiveFocus"));
    commitText(exposed.window(), QStringLiteral("Vega"));
    QTRY_COMPARE(searchModel->filterText(), QString("Vega"));

    QObject* toggle = firstObjectWithObjectName(root, QStringLiteral("searchToolbarToggle"));
    QVERIFY(toggle != nullptr);
    QVERIFY(activateControl(toggle));
    QTRY_VERIFY(controller->searchToolbarCollapsed());
    QTRY_COMPARE(searchField->property("text").toString(), QString());
    QCOMPARE(searchModel->filterText(), QString());

    QVERIFY(activateControl(toggle));
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
    auto* searchField = firstQuickItemWithObjectName(root, QStringLiteral("searchField"));
    QVERIFY(searchField != nullptr);

    QVERIFY(QMetaObject::invokeMethod(searchField, "forceActiveFocus"));
    commitText(exposed.window(), QStringLiteral("definitely-no-sky-object"));
    QTRY_COMPARE(searchModel->filterText(), QString("definitely-no-sky-object"));
    QTRY_COMPARE(searchModel->rowCount(), 0);
    auto* emptyLabel = firstQuickItemWithObjectName(root, QStringLiteral("searchEmptyLabel"));
    QVERIFY(emptyLabel != nullptr);
    QTRY_VERIFY(emptyLabel->isVisible());

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

    QObject* playPauseButton = firstObjectWithObjectName(
        root,
        QStringLiteral("timelinePlayPauseButton")
    );
    QVERIFY(playPauseButton != nullptr);
    QVERIFY(QMetaObject::invokeMethod(playPauseButton, "click"));
    QTRY_VERIFY(!controller->live());

    QVERIFY(QMetaObject::invokeMethod(playPauseButton, "click"));
    QTRY_VERIFY(controller->live());

    QObject* stepForwardButton = firstObjectWithObjectName(
        root,
        QStringLiteral("timelineStepForwardButton")
    );
    QVERIFY(stepForwardButton != nullptr);
    QVERIFY(QMetaObject::invokeMethod(stepForwardButton, "click"));
    QTRY_VERIFY(controller->utcTimeText() != originalTimeText);
    QObject* stepBackwardButton = firstObjectWithObjectName(
        root,
        QStringLiteral("timelineStepBackwardButton")
    );
    QVERIFY(stepBackwardButton != nullptr);
    QVERIFY(QMetaObject::invokeMethod(stepBackwardButton, "click"));
    QTRY_COMPARE(controller->utcTimeText(), originalTimeText);

    QObject* speedCombo = firstObjectWithObjectName(root, QStringLiteral("timelineSpeedCombo"));
    QObject* stepCombo = firstObjectWithObjectName(root, QStringLiteral("timelineStepCombo"));
    QObject* magnitudeCombo = firstObjectWithObjectName(
        root,
        QStringLiteral("timelineMagnitudeCombo")
    );
    QVERIFY(speedCombo != nullptr);
    QVERIFY(stepCombo != nullptr);
    QVERIFY(magnitudeCombo != nullptr);

    speedCombo->setProperty("currentIndex", 3);
    QVERIFY(QMetaObject::invokeMethod(speedCombo, "activated", Q_ARG(int, 3)));
    QCOMPARE(controller->speedMultiplier(), 2.0);

    stepCombo->setProperty("currentIndex", 4);
    QVERIFY(QMetaObject::invokeMethod(stepCombo, "activated", Q_ARG(int, 4)));
    QCOMPARE(controller->stepSeconds(), 3600);

    magnitudeCombo->setProperty("currentIndex", 5);
    QVERIFY(QMetaObject::invokeMethod(magnitudeCombo, "activated", Q_ARG(int, 5)));
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

    QObject* speedCombo = firstObjectWithObjectName(root, QStringLiteral("timelineSpeedCombo"));
    QObject* stepCombo = firstObjectWithObjectName(root, QStringLiteral("timelineStepCombo"));
    QObject* magnitudeCombo = firstObjectWithObjectName(
        root,
        QStringLiteral("timelineMagnitudeCombo")
    );
    QVERIFY(speedCombo != nullptr);
    QVERIFY(stepCombo != nullptr);
    QVERIFY(magnitudeCombo != nullptr);
    QTRY_COMPARE(speedCombo->property("currentIndex").toInt(), 5);
    QCOMPARE(stepCombo->property("currentIndex").toInt(), 4);
    QCOMPARE(magnitudeCombo->property("currentIndex").toInt(), 6);

    controller->setSpeedMultiplier(0.5);
    controller->setStepSeconds(10);
    controller->setMagnitudeCutoff(3.0);
    QTRY_COMPARE(speedCombo->property("currentIndex").toInt(), 1);
    QCOMPARE(stepCombo->property("currentIndex").toInt(), 1);
    QCOMPARE(magnitudeCombo->property("currentIndex").toInt(), 1);

    QObject* resetButton = firstObjectWithObjectName(
        root,
        QStringLiteral("timelineResetViewButton")
    );
    QVERIFY(resetButton != nullptr);
    QVERIFY(activateControl(resetButton));
    QTRY_COMPARE(controller->viewCenterAltitudeDeg(), 45.0);
    QCOMPARE(controller->viewCenterAzimuthDeg(), 180.0);

    QObject* toggle = firstObjectWithObjectName(root, QStringLiteral("timelineToolbarToggle"));
    QVERIFY(toggle != nullptr);
    QVERIFY(activateControl(toggle));
    QTRY_VERIFY(controller->timelineToolbarCollapsed());

    QVERIFY(activateControl(toggle));
    QTRY_VERIFY(!controller->timelineToolbarCollapsed());
    QCOMPARE(root->property("requestExpandCount").toInt(), 1);
    QVERIFY2(warnings.messages().isEmpty(), qPrintable(warnings.messages().join('\n')));
}

SKYGATE_QML_TEST_MAIN(QmlToolbarTests)

#include "QmlToolbarTests.moc"
