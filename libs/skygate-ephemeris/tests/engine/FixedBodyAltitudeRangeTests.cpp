#include "FixedBodyAltitudeRange.hpp"

#include <QtTest/QtTest>

class FixedBodyAltitudeRangeTests final : public QObject {
    Q_OBJECT

private slots:
    void computesAltitudeRangeForFixedBodies();
};

void FixedBodyAltitudeRangeTests::computesAltitudeRangeForFixedBodies()
{
    using skygate::ephemeris::FixedBodyAltitudeRange;

    QCOMPARE(FixedBodyAltitudeRange::maxAltitudeDeg(47.0, 60.0), 77.0);
    QCOMPARE(FixedBodyAltitudeRange::minAltitudeDeg(47.0, 60.0), 17.0);

    QCOMPARE(FixedBodyAltitudeRange::maxAltitudeDeg(47.0, -60.0), -17.0);
    QCOMPARE(FixedBodyAltitudeRange::minAltitudeDeg(47.0, -60.0), -77.0);

    QCOMPARE(FixedBodyAltitudeRange::maxAltitudeDeg(-33.0, -20.0), 77.0);
    QCOMPARE(FixedBodyAltitudeRange::minAltitudeDeg(-33.0, -20.0), -37.0);

    QCOMPARE(FixedBodyAltitudeRange::maxAltitudeDeg(0.0, 0.0), 90.0);
    QCOMPARE(FixedBodyAltitudeRange::minAltitudeDeg(0.0, 0.0), -90.0);
}

QTEST_APPLESS_MAIN(FixedBodyAltitudeRangeTests)

#include "FixedBodyAltitudeRangeTests.moc"
