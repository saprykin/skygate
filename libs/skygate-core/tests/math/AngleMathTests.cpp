#include "math/AngleMath.hpp"
#include "math/MathConstants.hpp"

#include <QtTest/QtTest>

#include <cmath>

namespace {

[[nodiscard]] bool isNear(const double actual, const double expected, const double tolerance = 1e-12)
{
    return std::abs(actual - expected) <= tolerance;
}

}  // namespace

class AngleMathTests final : public QObject {
    Q_OBJECT

private slots:
    void convertsBetweenDegreesAndRadians();
    void convertsBetweenHoursRadiansAndDegrees();
    void normalizesUnsignedDegreesAtBoundaries();
    void normalizesSignedDegreesAtBoundaries();
    void normalizesHoursAtDayBoundaries();
};

void AngleMathTests::convertsBetweenDegreesAndRadians()
{
    QVERIFY(isNear(skygate::core::AngleMath::toRadians(180.0), skygate::core::MathConstants::kPi));
    QVERIFY(isNear(skygate::core::AngleMath::toDegrees(skygate::core::MathConstants::kPi), 180.0));
}

void AngleMathTests::convertsBetweenHoursRadiansAndDegrees()
{
    QVERIFY(isNear(skygate::core::AngleMath::hoursToRadians(12.0), skygate::core::MathConstants::kPi));
    QVERIFY(isNear(skygate::core::AngleMath::radiansToHours(skygate::core::MathConstants::kPi), 12.0));
    QVERIFY(isNear(skygate::core::AngleMath::hoursToDegrees(6.0), 90.0));
    QVERIFY(isNear(skygate::core::AngleMath::degreesToHours(90.0), 6.0));

    for (const double hours : {0.0, 1.5, 6.0, 23.75}) {
        QVERIFY(isNear(
            skygate::core::AngleMath::radiansToHours(skygate::core::AngleMath::hoursToRadians(hours)), hours, 1e-12
        ));
        QVERIFY(isNear(
            skygate::core::AngleMath::degreesToHours(skygate::core::AngleMath::hoursToDegrees(hours)), hours, 1e-12
        ));
    }
}

void AngleMathTests::normalizesUnsignedDegreesAtBoundaries()
{
    QCOMPARE(skygate::core::AngleMath::normalizeDegrees(0.0), 0.0);
    QCOMPARE(skygate::core::AngleMath::normalizeDegrees(180.0), 180.0);
    QCOMPARE(skygate::core::AngleMath::normalizeDegrees(360.0), 0.0);
    QCOMPARE(skygate::core::AngleMath::normalizeDegrees(-360.0), -0.0);
    QCOMPARE(skygate::core::AngleMath::normalizeDegrees(721.5), 1.5);
    QCOMPARE(skygate::core::AngleMath::normalizeDegrees(-1.5), 358.5);
}

void AngleMathTests::normalizesSignedDegreesAtBoundaries()
{
    QCOMPARE(skygate::core::AngleMath::normalizeDegreesSigned(0.0), 0.0);
    QCOMPARE(skygate::core::AngleMath::normalizeDegreesSigned(180.0), 180.0);
    QCOMPARE(skygate::core::AngleMath::normalizeDegreesSigned(181.0), -179.0);
    QCOMPARE(skygate::core::AngleMath::normalizeDegreesSigned(360.0), 0.0);
    QCOMPARE(skygate::core::AngleMath::normalizeDegreesSigned(-180.0), 180.0);
    QCOMPARE(skygate::core::AngleMath::normalizeDegreesSigned(-181.0), 179.0);
}

void AngleMathTests::normalizesHoursAtDayBoundaries()
{
    QCOMPARE(skygate::core::AngleMath::normalizeHours(0.0), 0.0);
    QCOMPARE(skygate::core::AngleMath::normalizeHours(12.0), 12.0);
    QCOMPARE(skygate::core::AngleMath::normalizeHours(24.0), 0.0);
    QCOMPARE(skygate::core::AngleMath::normalizeHours(-24.0), -0.0);
    QCOMPARE(skygate::core::AngleMath::normalizeHours(49.25), 1.25);
    QCOMPARE(skygate::core::AngleMath::normalizeHours(-1.25), 22.75);
}

QTEST_APPLESS_MAIN(AngleMathTests)

#include "AngleMathTests.moc"
