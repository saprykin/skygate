#include <QtTest/QtTest>

#include "engine/highprecision/ErfaAstrometry.hpp"

class HighPrecisionErfaSmokeTests final : public QObject {
    Q_OBJECT

private slots:
    void convertsGregorianCalendarDateToJulianDate();
    void rejectsInvalidCalendarDate();
};

void HighPrecisionErfaSmokeTests::convertsGregorianCalendarDateToJulianDate()
{
    const auto julianDate = skygate::ephemeris::highprecision::calendarDateToJulianDate(2000, 1, 1);

    QVERIFY(julianDate.has_value());
    QCOMPARE(julianDate->day1, 2400000.5);
    QCOMPARE(julianDate->day2, 51544.0);
}

void HighPrecisionErfaSmokeTests::rejectsInvalidCalendarDate()
{
    QVERIFY(!skygate::ephemeris::highprecision::calendarDateToJulianDate(2000, 2, 30).has_value());
}

QTEST_MAIN(HighPrecisionErfaSmokeTests)

#include "HighPrecisionErfaSmokeTests.moc"
