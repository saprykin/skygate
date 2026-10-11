#include "engine/EphemerisDataStaleness.hpp"

#include <QtTest/QtTest>

#include <limits>
#include <optional>

namespace {

[[nodiscard]] skygate::core::AstronomicalEpoch utcEpoch(const double julianDate)
{
    return skygate::core::AstronomicalEpoch{
        .julianDatePart1 = julianDate, .julianDatePart2 = 0.0, .timeScale = skygate::core::TimeScale::Utc
    };
}

}  // namespace

class EphemerisDataStalenessTests final : public QObject {
    Q_OBJECT

private slots:
    void flagsReferenceEpochsAfterExpiration();
    void treatsExpirationInstantAsCurrent();
    void ignoresMissingEpochs();
    void ignoresNonUtcReferenceEpochs();
    void ignoresNonFiniteReferenceEpochs();
};

void EphemerisDataStalenessTests::flagsReferenceEpochsAfterExpiration()
{
    using skygate::ephemeris::EphemerisDataStaleness;

    const std::optional<skygate::core::AstronomicalEpoch> referenceEpoch = utcEpoch(2451546.0);
    const std::optional<skygate::core::AstronomicalEpoch> expiresAt = utcEpoch(2451545.0);

    QVERIFY(EphemerisDataStaleness::isStale(referenceEpoch, expiresAt));
}

void EphemerisDataStalenessTests::treatsExpirationInstantAsCurrent()
{
    using skygate::ephemeris::EphemerisDataStaleness;

    const std::optional<skygate::core::AstronomicalEpoch> referenceEpoch = utcEpoch(2451545.0);

    QVERIFY(!EphemerisDataStaleness::isStale(referenceEpoch, utcEpoch(2451545.0)));
    QVERIFY(!EphemerisDataStaleness::isStale(referenceEpoch, utcEpoch(2451545.5)));
}

void EphemerisDataStalenessTests::ignoresMissingEpochs()
{
    using skygate::ephemeris::EphemerisDataStaleness;

    QVERIFY(!EphemerisDataStaleness::isStale(std::nullopt, utcEpoch(2451545.0)));
    QVERIFY(!EphemerisDataStaleness::isStale(utcEpoch(2451546.0), std::nullopt));
    QVERIFY(!EphemerisDataStaleness::isStale(std::nullopt, std::nullopt));
}

void EphemerisDataStalenessTests::ignoresNonUtcReferenceEpochs()
{
    using skygate::ephemeris::EphemerisDataStaleness;

    skygate::core::AstronomicalEpoch taiEpoch = utcEpoch(2451546.0);
    taiEpoch.timeScale = skygate::core::TimeScale::Tai;

    QVERIFY(!EphemerisDataStaleness::isStale(taiEpoch, utcEpoch(2451545.0)));
}

void EphemerisDataStalenessTests::ignoresNonFiniteReferenceEpochs()
{
    using skygate::ephemeris::EphemerisDataStaleness;

    const std::optional<skygate::core::AstronomicalEpoch> infiniteEpoch =
        utcEpoch(std::numeric_limits<double>::infinity());
    const std::optional<skygate::core::AstronomicalEpoch> nanEpoch = utcEpoch(std::numeric_limits<double>::quiet_NaN());

    QVERIFY(!EphemerisDataStaleness::isStale(infiniteEpoch, utcEpoch(2451545.0)));
    QVERIFY(!EphemerisDataStaleness::isStale(nanEpoch, utcEpoch(2451545.0)));
}

QTEST_MAIN(EphemerisDataStalenessTests)

#include "EphemerisDataStalenessTests.moc"
