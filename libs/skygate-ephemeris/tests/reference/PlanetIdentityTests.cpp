#include "PlanetIdentity.hpp"
#include "UtcTimePoint.hpp"
#include "catalog/CelestialBodyIds.hpp"
#include "reference/PlanetEquatorialCalculator.hpp"

#include <QtTest/QtTest>

#include <array>
#include <chrono>
#include <optional>
#include <string_view>

namespace {

using skygate::ephemeris::CelestialBodyIds;
using skygate::ephemeris::PlanetIdentity;

struct PlanetCase final {
    std::string_view bodyId;
    int naifBodyCenterId = 0;
    int naifBarycenterId = 0;
    bool hasDe440sBodyCenter = false;
};

// Mixed-case ids prove the case-insensitive matching rule, including Pluto,
// which is not part of the bundled catalog vocabulary.
constexpr std::array<PlanetCase, 8> kPlanetCases = {{
    {.bodyId = "mercury", .naifBodyCenterId = 199, .naifBarycenterId = 1, .hasDe440sBodyCenter = true},
    {.bodyId = "VENUS", .naifBodyCenterId = 299, .naifBarycenterId = 2, .hasDe440sBodyCenter = true},
    {.bodyId = "Mars", .naifBodyCenterId = 499, .naifBarycenterId = 4},
    {.bodyId = "jupiter", .naifBodyCenterId = 599, .naifBarycenterId = 5},
    {.bodyId = "SATURN", .naifBodyCenterId = 699, .naifBarycenterId = 6},
    {.bodyId = "Uranus", .naifBodyCenterId = 799, .naifBarycenterId = 7},
    {.bodyId = "neptune", .naifBodyCenterId = 899, .naifBarycenterId = 8},
    {.bodyId = "PLUTO", .naifBodyCenterId = 999, .naifBarycenterId = 9},
}};

struct CatalogPlanetCase final {
    std::string_view bodyId;
    PlanetIdentity::Key key;
};

// Every planet id of the bundled catalog vocabulary must resolve to the same
// planet in the engines; the vocabulary lives in CelestialBodyIds only.
constexpr std::array<CatalogPlanetCase, 7> kCatalogPlanetCases = {{
    {.bodyId = CelestialBodyIds::kMercury, .key = PlanetIdentity::Key::Mercury},
    {.bodyId = CelestialBodyIds::kVenus, .key = PlanetIdentity::Key::Venus},
    {.bodyId = CelestialBodyIds::kMars, .key = PlanetIdentity::Key::Mars},
    {.bodyId = CelestialBodyIds::kJupiter, .key = PlanetIdentity::Key::Jupiter},
    {.bodyId = CelestialBodyIds::kSaturn, .key = PlanetIdentity::Key::Saturn},
    {.bodyId = CelestialBodyIds::kUranus, .key = PlanetIdentity::Key::Uranus},
    {.bodyId = CelestialBodyIds::kNeptune, .key = PlanetIdentity::Key::Neptune},
}};

}  // namespace

class PlanetIdentityTests final : public QObject {
    Q_OBJECT

private slots:
    void recognizesPlanetIdsIgnoringCase();
    void recognizesCatalogBodyIds();
    void marksPlanetsWithDe440sBodyCenters();
    void referenceCalculatorRecognizesMixedCaseIds();
};

void PlanetIdentityTests::recognizesPlanetIdsIgnoringCase()
{
    for (const PlanetCase& item : kPlanetCases) {
        const std::optional<PlanetIdentity> identity = PlanetIdentity::fromBodyId(item.bodyId);

        QVERIFY(identity.has_value());
        QCOMPARE(identity->naifBodyCenterId(), item.naifBodyCenterId);
        QCOMPARE(identity->naifBarycenterId(), item.naifBarycenterId);
    }

    QVERIFY(!PlanetIdentity::fromBodyId("earth").has_value());
    QVERIFY(!PlanetIdentity::fromBodyId("").has_value());
}

void PlanetIdentityTests::recognizesCatalogBodyIds()
{
    for (const CatalogPlanetCase& item : kCatalogPlanetCases) {
        const std::optional<PlanetIdentity> identity = PlanetIdentity::fromBodyId(item.bodyId);

        QVERIFY(identity.has_value());
        QVERIFY(identity->key() == item.key);
    }

    // The catalog's non-planet bodies stay outside the planet vocabulary.
    QVERIFY(!PlanetIdentity::fromBodyId(CelestialBodyIds::kSun).has_value());
    QVERIFY(!PlanetIdentity::fromBodyId(CelestialBodyIds::kMoon).has_value());
}

void PlanetIdentityTests::marksPlanetsWithDe440sBodyCenters()
{
    for (const PlanetCase& item : kPlanetCases) {
        const std::optional<PlanetIdentity> identity = PlanetIdentity::fromBodyId(item.bodyId);

        QVERIFY(identity.has_value());
        QCOMPARE(identity->hasDe440sBodyCenter(), item.hasDe440sBodyCenter);
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
