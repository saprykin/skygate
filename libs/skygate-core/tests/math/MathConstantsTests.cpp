#include "math/MathConstants.hpp"

#include <QtTest/QtTest>

#include <cmath>

namespace {

[[nodiscard]] bool isNear(const double actual, const double expected, const double tolerance = 1e-12)
{
    return std::abs(actual - expected) <= tolerance;
}

}  // namespace

class MathConstantsTests final : public QObject {
    Q_OBJECT

private slots:
    void kPi_hasExpectedValue();
    void kTwoPi_isDoubleOfkPi();
    void kDegreesToRadians_isInverseOfkRadiansToDegrees();
    void kRadiansPerHour_isPiOverTwelve();
    void kHoursPerRadian_isTwelveOverPi();
    void kArcsecondsToRadians_isPiOver180Over3600();
    void kMilliarcsecondsToRadians_isArcsecondsToRadiansOverThousand();
    void kEpsilon_isPositiveAndSmall();
};

void MathConstantsTests::kPi_hasExpectedValue()
{
    constexpr double kExpectedPi = 3.14159265358979323846;
    QVERIFY(isNear(skygate::core::MathConstants::kPi, kExpectedPi));
}

void MathConstantsTests::kTwoPi_isDoubleOfkPi()
{
    QVERIFY(isNear(skygate::core::MathConstants::kTwoPi, 2.0 * skygate::core::MathConstants::kPi));
}

void MathConstantsTests::kDegreesToRadians_isInverseOfkRadiansToDegrees()
{
    QVERIFY(
        isNear(skygate::core::MathConstants::kDegreesToRadians * skygate::core::MathConstants::kRadiansToDegrees, 1.0)
    );
}

void MathConstantsTests::kRadiansPerHour_isPiOverTwelve()
{
    QVERIFY(isNear(skygate::core::MathConstants::kRadiansPerHour, skygate::core::MathConstants::kPi / 12.0));
}

void MathConstantsTests::kHoursPerRadian_isTwelveOverPi()
{
    QVERIFY(isNear(skygate::core::MathConstants::kHoursPerRadian, 12.0 / skygate::core::MathConstants::kPi));
}

void MathConstantsTests::kArcsecondsToRadians_isPiOver180Over3600()
{
    QVERIFY(
        isNear(skygate::core::MathConstants::kArcsecondsToRadians, skygate::core::MathConstants::kPi / (180.0 * 3600.0))
    );
}

void MathConstantsTests::kMilliarcsecondsToRadians_isArcsecondsToRadiansOverThousand()
{
    QVERIFY(isNear(
        skygate::core::MathConstants::kMilliarcsecondsToRadians,
        skygate::core::MathConstants::kArcsecondsToRadians / 1000.0
    ));
}

void MathConstantsTests::kEpsilon_isPositiveAndSmall()
{
    QVERIFY(skygate::core::MathConstants::kEpsilon > 0.0);
    QVERIFY(skygate::core::MathConstants::kEpsilon < 1.0e-10);
}

QTEST_APPLESS_MAIN(MathConstantsTests)

#include "MathConstantsTests.moc"
