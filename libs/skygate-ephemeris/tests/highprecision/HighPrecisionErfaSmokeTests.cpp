#include <QtTest/QtTest>

#include "engine/highprecision/ErfaAstrometry.hpp"

class HighPrecisionErfaSmokeTests final : public QObject {
    Q_OBJECT

private slots:
    void rejectsWrongTimeScaleForTerrestrialTime();
    void rejectsWrongTimeScaleForUniversalTime1();
};

void HighPrecisionErfaSmokeTests::rejectsWrongTimeScaleForTerrestrialTime()
{
    const skygate::ephemeris::AstronomicalEpoch utcEpoch{
        .julianDatePart1 = 2400000.5,
        .julianDatePart2 = 51544.0,
        .timeScale = skygate::ephemeris::TimeScale::Utc,
    };

    QVERIFY(!skygate::ephemeris::highprecision::ErfaAstrometry::celestialToIntermediateMatrix06A(utcEpoch).has_value());
    QVERIFY(!skygate::ephemeris::highprecision::ErfaAstrometry::precessionNutationMatrix06A(utcEpoch).has_value());
    QVERIFY(!skygate::ephemeris::highprecision::ErfaAstrometry::tioLocatorS00(utcEpoch).has_value());
    QVERIFY(!skygate::ephemeris::highprecision::ErfaAstrometry::tdbMinusTtSeconds(utcEpoch, 0.0).has_value());
}

void HighPrecisionErfaSmokeTests::rejectsWrongTimeScaleForUniversalTime1()
{
    const skygate::ephemeris::AstronomicalEpoch ttEpoch{
        .julianDatePart1 = 2400000.5,
        .julianDatePart2 = 51544.0,
        .timeScale = skygate::ephemeris::TimeScale::Tt,
    };

    QVERIFY(!skygate::ephemeris::highprecision::ErfaAstrometry::earthRotationAngle00(ttEpoch).has_value());
}

QTEST_MAIN(HighPrecisionErfaSmokeTests)

#include "HighPrecisionErfaSmokeTests.moc"
