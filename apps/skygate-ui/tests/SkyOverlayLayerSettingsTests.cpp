#include <QtTest>

#include "SkyOverlayLayerSettings.hpp"

#include <QSignalSpy>

class SkyOverlayLayerSettingsTests final : public QObject {
    Q_OBJECT

private slots:
    void defaultsMatchLayerPolicy_data();
    void defaultsMatchLayerPolicy();
    void propertySettersRoundTrip_data();
    void propertySettersRoundTrip();
    void setVisibilityEmitsChangedSignalsOnce_data();
    void setVisibilityEmitsChangedSignalsOnce();
};

namespace {

void addOverlayLayerPropertyColumns()
{
    QTest::addColumn<QByteArray>("propertyName");
    QTest::addColumn<QByteArray>("signalName");
    QTest::addColumn<bool>("defaultValue");
    QTest::addColumn<bool>("changedValue");
}

void addOverlayLayerPropertyRows()
{
    QTest::newRow("horizon")
        << QByteArray("horizon") << QByteArray("horizonChanged") << true << false;
    QTest::newRow("alt-az-grid")
        << QByteArray("altAzGrid") << QByteArray("altAzGridChanged") << true << false;
    QTest::newRow("constellation-lines")
        << QByteArray("constellationLines")
        << QByteArray("constellationLinesChanged")
        << true
        << false;
    QTest::newRow("constellation-labels")
        << QByteArray("constellationLabels")
        << QByteArray("constellationLabelsChanged")
        << true
        << false;
    QTest::newRow("ecliptic")
        << QByteArray("ecliptic") << QByteArray("eclipticChanged") << false << true;
    QTest::newRow("celestial-equator")
        << QByteArray("celestialEquator")
        << QByteArray("celestialEquatorChanged")
        << false
        << true;
    QTest::newRow("circumpolar-boundary")
        << QByteArray("circumpolarBoundary")
        << QByteArray("circumpolarBoundaryChanged")
        << false
        << true;
    QTest::newRow("solar-system-labels")
        << QByteArray("solarSystemLabels")
        << QByteArray("solarSystemLabelsChanged")
        << true
        << false;
    QTest::newRow("deep-sky-objects")
        << QByteArray("deepSkyObjects")
        << QByteArray("deepSkyObjectsChanged")
        << true
        << false;
    QTest::newRow("deep-sky-labels")
        << QByteArray("deepSkyLabels") << QByteArray("deepSkyLabelsChanged") << true << false;
}

QMetaMethod signalMethod(const QObject& object, const QByteArray& signalName)
{
    const QByteArray signature = signalName + QByteArrayLiteral("()");
    const int signalIndex = object.metaObject()->indexOfSignal(signature.constData());
    Q_ASSERT(signalIndex >= 0);
    return object.metaObject()->method(signalIndex);
}

}  // namespace

void SkyOverlayLayerSettingsTests::defaultsMatchLayerPolicy_data()
{
    addOverlayLayerPropertyColumns();
    addOverlayLayerPropertyRows();
}

void SkyOverlayLayerSettingsTests::defaultsMatchLayerPolicy()
{
    QFETCH(QByteArray, propertyName);
    QFETCH(bool, defaultValue);

    SkyOverlayLayerSettings settings;
    QCOMPARE(settings.property(propertyName.constData()).toBool(), defaultValue);
}

void SkyOverlayLayerSettingsTests::propertySettersRoundTrip_data()
{
    addOverlayLayerPropertyColumns();
    addOverlayLayerPropertyRows();
}

void SkyOverlayLayerSettingsTests::propertySettersRoundTrip()
{
    QFETCH(QByteArray, propertyName);
    QFETCH(QByteArray, signalName);
    QFETCH(bool, defaultValue);
    QFETCH(bool, changedValue);

    SkyOverlayLayerSettings settings;
    QSignalSpy propertySpy(&settings, signalMethod(settings, signalName));
    QSignalSpy visibilitySpy(&settings, &SkyOverlayLayerSettings::visibilityChanged);

    QCOMPARE(settings.property(propertyName.constData()).toBool(), defaultValue);
    QVERIFY(settings.setProperty(propertyName.constData(), changedValue));

    QCOMPARE(settings.property(propertyName.constData()).toBool(), changedValue);
    QCOMPARE(propertySpy.count(), 1);
    QCOMPARE(visibilitySpy.count(), 1);

    QVERIFY(settings.setProperty(propertyName.constData(), changedValue));

    QCOMPARE(propertySpy.count(), 1);
    QCOMPARE(visibilitySpy.count(), 1);
}

void SkyOverlayLayerSettingsTests::setVisibilityEmitsChangedSignalsOnce_data()
{
    addOverlayLayerPropertyColumns();
    addOverlayLayerPropertyRows();
}

void SkyOverlayLayerSettingsTests::setVisibilityEmitsChangedSignalsOnce()
{
    QFETCH(QByteArray, propertyName);
    QFETCH(QByteArray, signalName);
    QFETCH(bool, changedValue);

    SkyOverlayLayerSettings settings;
    QSignalSpy propertySpy(&settings, signalMethod(settings, signalName));
    QSignalSpy visibilitySpy(&settings, &SkyOverlayLayerSettings::visibilityChanged);

    SkyOverlayLayerVisibility visibility = settings.visibility();
    QVERIFY(settings.setProperty(propertyName.constData(), changedValue));
    visibility = settings.visibility();
    QVERIFY(settings.setProperty(propertyName.constData(), !changedValue));
    propertySpy.clear();
    visibilitySpy.clear();

    settings.setVisibility(visibility);

    QCOMPARE(settings.property(propertyName.constData()).toBool(), changedValue);
    QCOMPARE(propertySpy.count(), 1);
    QCOMPARE(visibilitySpy.count(), 1);

    settings.setVisibility(visibility);

    QCOMPARE(propertySpy.count(), 1);
    QCOMPARE(visibilitySpy.count(), 1);
}

QTEST_GUILESS_MAIN(SkyOverlayLayerSettingsTests)

#include "SkyOverlayLayerSettingsTests.moc"
