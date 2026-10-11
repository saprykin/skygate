#include "PlanetIdentity.hpp"
#include "UtcTimePoint.hpp"
#include "reference/PlanetEquatorialCalculator.hpp"

#include <QtTest/QtTest>

#include <array>
#include <chrono>
#include <optional>
#include <string_view>

namespace {

using skygate::ephemeris::PlanetIdentity;

struct PlanetCase final {
    std::string_view bodyId;
    std::string_view canonicalId;
    int naifBodyCenterId = 0;
    int naifBarycenterId = 0;
};

constexpr std::array<PlanetCase, 8> kPlanetCases = {{
    {.bodyId = "mercury", .canonicalId = "mercury", .naifBodyCenterId = 199, .naifBarycenterId = 1},
    {.bodyId = "VENUS", .canonicalId = "venus", .naifBodyCenterId = 299, .naifBarycenterId = 2},
    {.bodyId = "Mars", .canonicalId = "mars", .naifBodyCenterId = 499, .naifBarycenterId = 4},
    {.bodyId = "jupiter", .canonicalId = "jupiter", .naifBodyCenterId = 599, .naifBarycenterId = 5},
    {.bodyId = "SATURN", .canonicalId = "saturn", .naifBodyCenterId = 699, .naifBarycenterId = 6},
    {.bodyId = "Uranus", .canonicalId = "uranus", .naifBodyCenterId = 799, .naifBarycenterId = 7},
    {.bodyId = "neptune", .canonicalId = "neptune", .naifBodyCenterId = 899, .naifBarycenterId = 8},
    {.bodyId = "PLUTO", .canonicalId = "pluto", .naifBodyCenterId = 999, .naifBarycenterId = 9},
}};

}  // namespace

class PlanetIdentityTests final : public QObject {
    Q_OBJECT

private slots:
    void recognizesPlanetIdsIgnoringCase();
    void marksPlanetsWithDe440sBodyCenters();
    void referenceCalculatorRecognizesMixedCaseIds();
};

void PlanetIdentityTests::recognizesPlanetIdsIgnoringCase()
{
    for (const PlanetCase& item : kPlanetCases) {
        const std::optional<PlanetIdentity> identity = PlanetIdentity::fromBodyId(item.bodyId);

        QVERIFY(identity.has_value());
        QVERIFY(identity->canonicalId() == item.canonicalId);
        QCOMPARE(identity->naifBodyCenterId(), item.naifBodyCenterId);
        QCOMPARE(identity->naifBarycenterId(), item.naifBarycenterId);
    }

    QVERIFY(!PlanetIdentity::fromBodyId("earth").has_value());
    QVERIFY(!PlanetIdentity::fromBodyId("sun").has_value());
    QVERIFY(!PlanetIdentity::fromBodyId("").has_value());
}

void PlanetIdentityTests::marksPlanetsWithDe440sBodyCenters()
{
    for (const PlanetCase& item : kPlanetCases) {
        const std::optional<PlanetIdentity> identity = PlanetIdentity::fromBodyId(item.bodyId);

        QVERIFY(identity.has_value());
        const bool expected = item.canonicalId == "mercury" || item.canonicalId == "venus";
        QCOMPARE(identity->hasDe440sBodyCenter(), expected);
    }
}

void PlanetIdentityTests::referenceCalculatorRecognizesMixedCaseIds()
{
    const skygate::ephemeris::PlanetEquatorialCalculator calculator;
    const skygate::core::UtcTimePoint utcTime(std::chrono::seconds(1'704'067'200));

    const std::optional<skygate::core::EquatorialCoordinate> mixedCase = calculator.compute("MARS", utcTime);
    const std::optional<skygate::core::EquatorialCoordinate> canonical = calculator.compute("mars", utcTime);
    QVERIFY(mixedCase.has_value());
    QVERIFY(canonical.has_value());
    QCOMPARE(mixedCase->rightAscensionHours, canonical->rightAscensionHours);
    QCOMPARE(mixedCase->declinationDeg, canonical->declinationDeg);

    QVERIFY(!calculator.compute("earth", utcTime).has_value());
}

QTEST_APPLESS_MAIN(PlanetIdentityTests)

#include "PlanetIdentityTests.moc"
