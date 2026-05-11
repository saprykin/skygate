#include "QmlPreferencesTestSupport.hpp"

class QmlPreferencesWindowTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void preferencesWindowShellNavigatesAppliesAndCloses();

private:
    QmlSettingsFixture m_settings;
};

void QmlPreferencesWindowTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("QmlPreferencesWindowTests")));
}

void QmlPreferencesWindowTests::init()
{
    m_settings.resetForCurrentTest();
}

void QmlPreferencesWindowTests::preferencesWindowShellNavigatesAppliesAndCloses()
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
        QStringLiteral("windows/PreferencesWindow.qml"),
        {{QStringLiteral("skyContextController"), QVariant::fromValue<QObject*>(controller.get())}}
    );
    QVERIFY(object != nullptr);
    auto* window = qobject_cast<QQuickWindow*>(object.get());
    QVERIFY(window != nullptr);

    QObject* draft = firstObjectWithObjectName(window, QStringLiteral("preferencesDraft"));
    QVERIFY(draft != nullptr);
    draft->setProperty("latitudeText", QStringLiteral("1.000000"));

    QTest::ignoreMessage(QtWarningMsg, "This plugin does not support raise()");
    QVERIFY(QMetaObject::invokeMethod(window, "openWindow"));
    QTRY_VERIFY(window->isVisible());
    QCOMPARE(draft->property("latitudeText").toString(), QString("47.000000"));

    QObject* catalogSection = firstObjectWithObjectName(
        window,
        QStringLiteral("preferencesCatalogSectionButton")
    );
    QVERIFY(catalogSection != nullptr);
    QVERIFY(activateControl(catalogSection));
    QTRY_COMPARE(window->property("selectedPage").toInt(), 3);
    QCOMPARE(
        window->property("currentSectionDescription").toString(),
        QString("Catalog source and download settings")
    );

    QObject* skySection = firstObjectWithObjectName(
        window,
        QStringLiteral("preferencesSkySectionButton")
    );
    QVERIFY(skySection != nullptr);
    QVERIFY(activateControl(skySection));
    QTRY_COMPARE(window->property("selectedPage").toInt(), 1);

    QObject* applyButton = firstObjectWithObjectName(
        window,
        QStringLiteral("preferencesApplyButton")
    );
    QVERIFY(applyButton != nullptr);
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

SKYGATE_QML_TEST_MAIN(QmlPreferencesWindowTests)

#include "QmlPreferencesWindowTests.moc"
