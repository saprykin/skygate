#include <QtTest>

#include "SkyOverlayLayerSettings.hpp"

#include <QSignalSpy>

class SkyOverlayLayerSettingsTests final : public QObject {
    Q_OBJECT

private slots:
    void defaultsMatchLayerPolicy();
    void settersEmitSpecificAndAggregateSignals();
    void setVisibilityEmitsChangedSignalsOnce();
};

void SkyOverlayLayerSettingsTests::defaultsMatchLayerPolicy()
{
    SkyOverlayLayerSettings settings;

    QVERIFY(settings.horizon());
    QVERIFY(settings.altAzGrid());
    QVERIFY(settings.constellationLines());
    QVERIFY(settings.constellationLabels());
    QVERIFY(!settings.ecliptic());
    QVERIFY(!settings.celestialEquator());
    QVERIFY(!settings.circumpolarBoundary());
    QVERIFY(settings.solarSystemLabels());
}

void SkyOverlayLayerSettingsTests::settersEmitSpecificAndAggregateSignals()
{
    SkyOverlayLayerSettings settings;
    QSignalSpy horizonSpy(&settings, &SkyOverlayLayerSettings::horizonChanged);
    QSignalSpy visibilitySpy(&settings, &SkyOverlayLayerSettings::visibilityChanged);

    settings.setHorizon(false);

    QVERIFY(!settings.horizon());
    QCOMPARE(horizonSpy.count(), 1);
    QCOMPARE(visibilitySpy.count(), 1);

    settings.setHorizon(false);

    QCOMPARE(horizonSpy.count(), 1);
    QCOMPARE(visibilitySpy.count(), 1);
}

void SkyOverlayLayerSettingsTests::setVisibilityEmitsChangedSignalsOnce()
{
    SkyOverlayLayerSettings settings;
    QSignalSpy horizonSpy(&settings, &SkyOverlayLayerSettings::horizonChanged);
    QSignalSpy eclipticSpy(&settings, &SkyOverlayLayerSettings::eclipticChanged);
    QSignalSpy visibilitySpy(&settings, &SkyOverlayLayerSettings::visibilityChanged);

    SkyOverlayLayerVisibility visibility = settings.visibility();
    visibility.horizon = false;
    visibility.ecliptic = true;
    settings.setVisibility(visibility);

    QCOMPARE(horizonSpy.count(), 1);
    QCOMPARE(eclipticSpy.count(), 1);
    QCOMPARE(visibilitySpy.count(), 1);
}

QTEST_GUILESS_MAIN(SkyOverlayLayerSettingsTests)

#include "SkyOverlayLayerSettingsTests.moc"
