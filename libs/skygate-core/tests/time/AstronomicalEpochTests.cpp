#include "time/AstronomicalEpoch.hpp"

#include <QtTest/QtTest>

#include <bit>
#include <cstdint>
#include <limits>

namespace {

constexpr double kJulianDatePart1 = 2'460'310.0;

}  // namespace

class AstronomicalEpochTests final : public QObject {
    Q_OBJECT

private slots:
    void isIdenticalToDistinguishesSignedZero();
    void isIdenticalToDistinguishesTimeScales();
    void isIdenticalToComparesNanBits();
};

void AstronomicalEpochTests::isIdenticalToDistinguishesSignedZero()
{
    const skygate::core::AstronomicalEpoch positiveZeroEpoch{
        .julianDatePart1 = kJulianDatePart1,
        .julianDatePart2 = 0.0,
        .timeScale = skygate::core::TimeScale::Tdb,
    };
    const skygate::core::AstronomicalEpoch negativeZeroEpoch{
        .julianDatePart1 = kJulianDatePart1,
        .julianDatePart2 = -0.0,
        .timeScale = skygate::core::TimeScale::Tdb,
    };

    QVERIFY(positiveZeroEpoch == negativeZeroEpoch);
    QVERIFY(!positiveZeroEpoch.isIdenticalTo(negativeZeroEpoch));
    QVERIFY(!negativeZeroEpoch.isIdenticalTo(positiveZeroEpoch));
    QVERIFY(positiveZeroEpoch.isIdenticalTo(positiveZeroEpoch));
}

void AstronomicalEpochTests::isIdenticalToDistinguishesTimeScales()
{
    const skygate::core::AstronomicalEpoch tdbEpoch{
        .julianDatePart1 = kJulianDatePart1,
        .julianDatePart2 = 0.5,
        .timeScale = skygate::core::TimeScale::Tdb,
    };
    skygate::core::AstronomicalEpoch utcEpoch = tdbEpoch;
    utcEpoch.timeScale = skygate::core::TimeScale::Utc;

    QVERIFY(tdbEpoch.isIdenticalTo(tdbEpoch));
    QVERIFY(!tdbEpoch.isIdenticalTo(utcEpoch));
}

void AstronomicalEpochTests::isIdenticalToComparesNanBits()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const skygate::core::AstronomicalEpoch nanEpoch{
        .julianDatePart1 = nan,
        .julianDatePart2 = 0.5,
        .timeScale = skygate::core::TimeScale::Tdb,
    };
    const skygate::core::AstronomicalEpoch sameNanEpoch = nanEpoch;
    skygate::core::AstronomicalEpoch otherNanEpoch = nanEpoch;
    otherNanEpoch.julianDatePart1 = std::bit_cast<double>(std::bit_cast<std::uint64_t>(nan) | 1ULL);

    QVERIFY(!(nanEpoch == sameNanEpoch));
    QVERIFY(nanEpoch.isIdenticalTo(sameNanEpoch));
    QVERIFY(!nanEpoch.isIdenticalTo(otherNanEpoch));
}

QTEST_APPLESS_MAIN(AstronomicalEpochTests)

#include "AstronomicalEpochTests.moc"
