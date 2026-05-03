#include "engine/CoordinateTransform.hpp"
#include "engine/JulianDateTime.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/CatalogFactory.hpp"

#include <QtTest/QtTest>

#include <chrono>
#include <cmath>
#include <string_view>

namespace {

[[nodiscard]] bool isNear(const double actual, const double expected, const double tolerance)
{
    return std::abs(actual - expected) <= tolerance;
}

const skygate::ephemeris::CelestialBodyState* findStateById(
    const skygate::ephemeris::SkySnapshot& snapshot,
    const std::string_view id
)
{
    if (snapshot.catalogBodies == nullptr) {
        return nullptr;
    }

    for (const auto& state : snapshot.states) {
        if (state.bodyIndex >= snapshot.catalogBodies->size()) {
            continue;
        }
        if (snapshot.catalogBodies->at(state.bodyIndex).id == id) {
            return &state;
        }
    }
    return nullptr;
}

void compareEquatorial(
    const skygate::core::EquatorialCoordinate& actual,
    const double expectedRightAscensionHours,
    const double expectedDeclinationDeg
)
{
    QVERIFY(isNear(actual.rightAscensionHours, expectedRightAscensionHours, 1e-9));
    QVERIFY(isNear(actual.declinationDeg, expectedDeclinationDeg, 1e-9));
}

}  // namespace

class EphemerisRegressionTests final : public QObject {
    Q_OBJECT

private slots:
    void solarSystemBodiesMatchGoldenApproximation();
    void equatorialToHorizontalPlacesTransitAtZenith();
    void observerLongitudeChangesLocalSiderealHourAngle();
};

void EphemerisRegressionTests::solarSystemBodiesMatchGoldenApproximation()
{
    const auto catalog = skygate::ephemeris::createBundledStarCatalog();
    QVERIFY(catalog != nullptr);

    const auto engine = skygate::ephemeris::createEphemerisEngine(*catalog);
    QVERIFY(engine != nullptr);

    skygate::core::SkyContext context;
    context.observer.latitudeDeg = 0.0;
    context.observer.longitudeDeg = 0.0;
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1'704'067'200));

    const auto snapshot = engine->compute(context);
    const struct {
        const char* id;
        double rightAscensionHours;
        double declinationDeg;
    } expectedBodies[] = {
        {"sun", 18.728370267444117, -23.05573891453204},
        {"moon", 9.508391936548342, 19.54533239410588},
        {"mercury", 8.416244848273136, 20.612871270493663},
        {"venus", 12.295206378874408, -3.5022633354619996},
        {"mars", 17.909109756416846, -22.0767869441085},
        {"jupiter", 2.701259642739681, 14.627484276161153},
        {"saturn", 22.995359968993508, -7.505707533115917},
        {"uranus", 3.632771322122132, 19.742364509621595},
        {"neptune", 23.79296443153429, -0.9436366168541259},
    };

    for (const auto& expectedBody : expectedBodies) {
        const auto* state = findStateById(snapshot, expectedBody.id);
        QVERIFY2(state != nullptr, expectedBody.id);
        compareEquatorial(
            state->equatorial,
            expectedBody.rightAscensionHours,
            expectedBody.declinationDeg
        );
    }
}

void EphemerisRegressionTests::equatorialToHorizontalPlacesTransitAtZenith()
{
    const skygate::core::UtcTimePoint j2000NoonUtc(std::chrono::seconds(946'728'000));
    const double transitRightAscensionHours =
        skygate::ephemeris::JulianDateTime::greenwichMeanSiderealTimeDeg(j2000NoonUtc) / 15.0;

    const auto horizontal = skygate::ephemeris::CoordinateTransform::equatorialToHorizontal(
        {.rightAscensionHours = transitRightAscensionHours, .declinationDeg = 0.0},
        {.latitudeDeg = 0.0, .longitudeDeg = 0.0, .elevationMeters = 0.0},
        j2000NoonUtc
    );

    QVERIFY(isNear(horizontal.altitudeDeg, 90.0, 1e-9));
    QVERIFY(horizontal.azimuthDeg >= 0.0);
    QVERIFY(horizontal.azimuthDeg < 360.0);
}

void EphemerisRegressionTests::observerLongitudeChangesLocalSiderealHourAngle()
{
    const skygate::core::UtcTimePoint j2000NoonUtc(std::chrono::seconds(946'728'000));
    const double greenwichRightAscensionHours =
        skygate::ephemeris::JulianDateTime::greenwichMeanSiderealTimeDeg(j2000NoonUtc) / 15.0;

    const auto greenwichHorizontal = skygate::ephemeris::CoordinateTransform::equatorialToHorizontal(
        {.rightAscensionHours = greenwichRightAscensionHours, .declinationDeg = 0.0},
        {.latitudeDeg = 0.0, .longitudeDeg = 0.0, .elevationMeters = 0.0},
        j2000NoonUtc
    );
    const auto eastLongitudeHorizontal = skygate::ephemeris::CoordinateTransform::equatorialToHorizontal(
        {.rightAscensionHours = greenwichRightAscensionHours, .declinationDeg = 0.0},
        {.latitudeDeg = 0.0, .longitudeDeg = 15.0, .elevationMeters = 0.0},
        j2000NoonUtc
    );

    QVERIFY(greenwichHorizontal.altitudeDeg > eastLongitudeHorizontal.altitudeDeg);
    QVERIFY(isNear(eastLongitudeHorizontal.altitudeDeg, 75.0, 1e-9));
    QVERIFY(isNear(eastLongitudeHorizontal.azimuthDeg, 270.0, 1e-9));
}

QTEST_APPLESS_MAIN(EphemerisRegressionTests)

#include "EphemerisRegressionTests.moc"
