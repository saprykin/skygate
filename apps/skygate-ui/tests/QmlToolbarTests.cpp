#include "QmlTestSupport.hpp"

using namespace skygate::ui::tests;

class QmlToolbarTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void searchToolbarFiltersActivatesAndClearsResults();
    void timelineToolbarControlsUpdateController();

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

void QmlToolbarTests::timelineToolbarControlsUpdateController()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    controller->setTimelineToolbarCollapsed(false);
    controller->setLive(true);
    controller->setStepSeconds(60);
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

SKYGATE_QML_TEST_MAIN(QmlToolbarTests)

#include "QmlToolbarTests.moc"
