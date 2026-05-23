#include "math/PhysicalConstants.hpp"

#include <QtTest/QtTest>

#include <cmath>

namespace {

[[nodiscard]] bool isNear(const double actual, const double expected, const double tolerance = 1e-12)
{
    return std::abs(actual - expected) <= tolerance;
}

}  // namespace

class PhysicalConstantsTests final : public QObject {
    Q_OBJECT

private slots:
    void kSpeedOfLightKms_hasExpectedValue();
    void kSpeedOfLightAuPerDay_hasExpectedValue();
    void kAstronomicalUnitMeters_hasExpectedValue();
    void kAstronomicalUnitKilometers_hasExpectedValue();
    void kAuPerParsec_hasExpectedValue();
    void kSolarSchwarzschildRadiusAu_hasExpectedValue();
    void kSynodicMonthDays_hasExpectedValue();
    void kAstronomicalUnit_isConsistentMetersToKilometers();
};

void PhysicalConstantsTests::kSpeedOfLightKms_hasExpectedValue()
{
    QCOMPARE(skygate::core::PhysicalConstants::kSpeedOfLightKms, 299'792.458);
}

void PhysicalConstantsTests::kSpeedOfLightAuPerDay_hasExpectedValue()
{
    QCOMPARE(skygate::core::PhysicalConstants::kSpeedOfLightAuPerDay, 173.144632674240);
}

void PhysicalConstantsTests::kAstronomicalUnitMeters_hasExpectedValue()
{
    QCOMPARE(skygate::core::PhysicalConstants::kAstronomicalUnitMeters, 149'597'870'700.0);
}

void PhysicalConstantsTests::kAstronomicalUnitKilometers_hasExpectedValue()
{
    QCOMPARE(skygate::core::PhysicalConstants::kAstronomicalUnitKilometers, 149'597'870.7);
}

void PhysicalConstantsTests::kAuPerParsec_hasExpectedValue()
{
    QCOMPARE(skygate::core::PhysicalConstants::kAuPerParsec, 206'264.80624709636);
}

void PhysicalConstantsTests::kSolarSchwarzschildRadiusAu_hasExpectedValue()
{
    QCOMPARE(skygate::core::PhysicalConstants::kSolarSchwarzschildRadiusAu, 1.97412574336e-8);
}

void PhysicalConstantsTests::kSynodicMonthDays_hasExpectedValue()
{
    QCOMPARE(skygate::core::PhysicalConstants::kSynodicMonthDays, 29.530588853);
    // Synodic month is between 29.2 and 29.9 days.
    QVERIFY(skygate::core::PhysicalConstants::kSynodicMonthDays > 29.0);
    QVERIFY(skygate::core::PhysicalConstants::kSynodicMonthDays < 30.0);
}

void PhysicalConstantsTests::kAstronomicalUnit_isConsistentMetersToKilometers()
{
    QVERIFY(isNear(
        skygate::core::PhysicalConstants::kAstronomicalUnitMeters,
        skygate::core::PhysicalConstants::kAstronomicalUnitKilometers * 1000.0
    ));
}

QTEST_APPLESS_MAIN(PhysicalConstantsTests)

#include "PhysicalConstantsTests.moc"
