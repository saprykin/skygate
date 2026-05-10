#include "engine/AstronomicalTime.hpp"
#include "engine/EquatorialToHorizontalCalculator.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/CatalogFactory.hpp"

#include <QtTest/QtTest>

#include <array>
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
    void solarSystemBodiesMatchGoldenApproximationAcrossContexts();
    void solarSystemBodiesStayNearExternalReferenceValues();
    void equatorialToHorizontalPlacesTransitAtZenith();
    void observerLongitudeChangesLocalSiderealHourAngle();
    void rightAscensionWrapsAcrossTwentyFourHours();
    void negativeLongitudeMirrorsPositiveLongitude();
    void polarObserverPlacesMatchingCelestialPoleAtZenith();
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

void EphemerisRegressionTests::solarSystemBodiesMatchGoldenApproximationAcrossContexts()
{
    const auto catalog = skygate::ephemeris::createBundledStarCatalog();
    QVERIFY(catalog != nullptr);

    const auto engine = skygate::ephemeris::createEphemerisEngine(*catalog);
    QVERIFY(engine != nullptr);

    struct ExpectedBody final {
        const char* id;
        double rightAscensionHours;
        double declinationDeg;
        double altitudeDeg;
        double azimuthDeg;
    };
    struct ExpectedContext final {
        const char* label;
        qint64 epochSeconds;
        double latitudeDeg;
        double longitudeDeg;
        std::array<ExpectedBody, 4> bodies;
    };

    const std::array<ExpectedContext, 4> expectedContexts {{
        {
            "j2000-greenwich",
            946'728'000,
            0.0,
            0.0,
            {{
                {"sun", 18.752390986788, -23.033722810174, 66.952303200219, 178.059827337618},
                {"moon", 13.606967187179, -4.620371228927, 13.598697679418, 265.246064746649},
                {"mars", 23.687156971582, -0.655487464939, 15.152248370149, 90.679097561788},
                {"jupiter", 2.162160182484, 12.036902928235, -21.464439720546, 77.051041427880},
            }}
        },
        {
            "sydney-2024",
            1'717'286'400,
            -33.8688,
            151.2093,
            {{
                {"sun", 4.697396799037, 22.220842004465, 27.778571639809, 29.733425054502},
                {"moon", 23.546345875272, -4.957712310191, 36.240663186815, 291.279394717935},
                {"mars", 23.283809279069, -3.097924348361, 32.014716927364, 290.052404659305},
                {"jupiter", 3.552228278130, 18.032469824940, 37.027768764624, 13.277231976213},
            }}
        },
        {
            "reykjavik-2030",
            1'893'456'000,
            64.1466,
            -21.9426,
            {{
                {"sun", 18.768374471267, -23.011989858818, -46.208921944971, 329.008763777304},
                {"moon", 14.320732179737, -17.848591793485, -35.080177268831, 53.812896434812},
                {"mars", 22.565909659142, -7.310952367138, -11.043830150106, 276.053220424013},
                {"jupiter", 14.841327733812, -15.247642372006, -35.203380284876, 44.120873138471},
            }}
        },
        {
            "nyc-1900",
            -2'208'988'800,
            40.7128,
            -74.0060,
            {{
                {"sun", 18.735908361734, -23.065717821748, -25.955890647458, 261.026576639059},
                {"moon", 16.626375237468, -22.841788231274, -49.664662514094, 282.838403672711},
                {"mars", 19.832366931208, -19.275478841654, -11.477689433846, 254.355453947332},
                {"jupiter", 15.850360928718, -19.024755381275, -55.396664886941, 299.338130366724},
            }}
        },
    }};

    for (const ExpectedContext& expectedContext : expectedContexts) {
        skygate::core::SkyContext context;
        context.observer.latitudeDeg = expectedContext.latitudeDeg;
        context.observer.longitudeDeg = expectedContext.longitudeDeg;
        context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(
            expectedContext.epochSeconds
        ));

        const auto snapshot = engine->compute(context);
        for (const ExpectedBody& expectedBody : expectedContext.bodies) {
            const auto* state = findStateById(snapshot, expectedBody.id);
            QVERIFY2(state != nullptr, expectedContext.label);
            QVERIFY2(
                isNear(
                    state->equatorial.rightAscensionHours,
                    expectedBody.rightAscensionHours,
                    1e-9
                ),
                expectedContext.label
            );
            QVERIFY2(
                isNear(state->equatorial.declinationDeg, expectedBody.declinationDeg, 1e-9),
                expectedContext.label
            );
            QVERIFY2(
                isNear(state->horizontal.altitudeDeg, expectedBody.altitudeDeg, 1e-9),
                expectedContext.label
            );
            QVERIFY2(
                isNear(state->horizontal.azimuthDeg, expectedBody.azimuthDeg, 1e-9),
                expectedContext.label
            );
        }
    }
}

void EphemerisRegressionTests::solarSystemBodiesStayNearExternalReferenceValues()
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
    struct ExternalReference final {
        const char* id;
        double rightAscensionHours;
        double declinationDeg;
        double altitudeDeg;
        double azimuthDeg;
        double equatorialToleranceDeg;
        double horizontalToleranceDeg;
    };

    // NASA/JPL Horizons observer table, 2024-Jan-01 00:00 UTC at 0 E, 0 N.
    // The production engine is intentionally low precision, so these guardrails
    // catch gross drift without requiring planetary-ephemeris accuracy.
    const std::array<ExternalReference, 3> references {{
        {"sun", 18.70435, -23.07967, -66.93037, 181.80724, 0.75, 1.0},
        {"moon", 10.63987, 12.85442, 29.41594, 75.34794, 25.0, 30.0},
        {"mars", 17.77972, -23.95164, -62.87989, 152.98409, 4.0, 35.0},
    }};

    for (const ExternalReference& reference : references) {
        const auto* state = findStateById(snapshot, reference.id);
        QVERIFY2(state != nullptr, reference.id);
        const double raDeltaHours = std::abs(
            state->equatorial.rightAscensionHours - reference.rightAscensionHours
        );
        const double wrappedRaDeltaHours = std::min(raDeltaHours, 24.0 - raDeltaHours);
        QVERIFY2(
            wrappedRaDeltaHours * 15.0 <= reference.equatorialToleranceDeg,
            reference.id
        );
        QVERIFY2(
            isNear(
                state->equatorial.declinationDeg,
                reference.declinationDeg,
                reference.equatorialToleranceDeg
            ),
            reference.id
        );
        QVERIFY2(
            isNear(
                state->horizontal.altitudeDeg,
                reference.altitudeDeg,
                reference.horizontalToleranceDeg
            ),
            reference.id
        );
        QVERIFY2(
            isNear(
                state->horizontal.azimuthDeg,
                reference.azimuthDeg,
                reference.horizontalToleranceDeg
            ),
            reference.id
        );
    }
}

void EphemerisRegressionTests::equatorialToHorizontalPlacesTransitAtZenith()
{
    const skygate::core::UtcTimePoint j2000NoonUtc(std::chrono::seconds(946'728'000));
    const double transitRightAscensionHours =
        skygate::ephemeris::AstronomicalTime::greenwichMeanSiderealTimeDeg(j2000NoonUtc) / 15.0;

    const auto horizontal = skygate::ephemeris::EquatorialToHorizontalCalculator::compute(
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
        skygate::ephemeris::AstronomicalTime::greenwichMeanSiderealTimeDeg(j2000NoonUtc) / 15.0;

    const auto greenwichHorizontal =
        skygate::ephemeris::EquatorialToHorizontalCalculator::compute(
        {.rightAscensionHours = greenwichRightAscensionHours, .declinationDeg = 0.0},
        {.latitudeDeg = 0.0, .longitudeDeg = 0.0, .elevationMeters = 0.0},
        j2000NoonUtc
    );
    const auto eastLongitudeHorizontal =
        skygate::ephemeris::EquatorialToHorizontalCalculator::compute(
        {.rightAscensionHours = greenwichRightAscensionHours, .declinationDeg = 0.0},
        {.latitudeDeg = 0.0, .longitudeDeg = 15.0, .elevationMeters = 0.0},
        j2000NoonUtc
    );

    QVERIFY(greenwichHorizontal.altitudeDeg > eastLongitudeHorizontal.altitudeDeg);
    QVERIFY(isNear(eastLongitudeHorizontal.altitudeDeg, 75.0, 1e-9));
    QVERIFY(isNear(eastLongitudeHorizontal.azimuthDeg, 270.0, 1e-9));
}

void EphemerisRegressionTests::rightAscensionWrapsAcrossTwentyFourHours()
{
    const skygate::core::UtcTimePoint j2000NoonUtc(std::chrono::seconds(946'728'000));

    const auto zeroRightAscension = skygate::ephemeris::EquatorialToHorizontalCalculator::compute(
        {.rightAscensionHours = 0.0, .declinationDeg = 0.0},
        {.latitudeDeg = 0.0, .longitudeDeg = 0.0, .elevationMeters = 0.0},
        j2000NoonUtc
    );
    const auto wrappedRightAscension =
        skygate::ephemeris::EquatorialToHorizontalCalculator::compute(
        {.rightAscensionHours = 24.0, .declinationDeg = 0.0},
        {.latitudeDeg = 0.0, .longitudeDeg = 0.0, .elevationMeters = 0.0},
        j2000NoonUtc
    );

    QVERIFY(isNear(wrappedRightAscension.altitudeDeg, zeroRightAscension.altitudeDeg, 1e-9));
    QVERIFY(isNear(wrappedRightAscension.azimuthDeg, zeroRightAscension.azimuthDeg, 1e-9));
}

void EphemerisRegressionTests::negativeLongitudeMirrorsPositiveLongitude()
{
    const skygate::core::UtcTimePoint j2000NoonUtc(std::chrono::seconds(946'728'000));
    const double greenwichRightAscensionHours =
        skygate::ephemeris::AstronomicalTime::greenwichMeanSiderealTimeDeg(j2000NoonUtc) / 15.0;

    const auto eastLongitudeHorizontal =
        skygate::ephemeris::EquatorialToHorizontalCalculator::compute(
        {.rightAscensionHours = greenwichRightAscensionHours, .declinationDeg = 0.0},
        {.latitudeDeg = 0.0, .longitudeDeg = 15.0, .elevationMeters = 0.0},
        j2000NoonUtc
    );
    const auto westLongitudeHorizontal =
        skygate::ephemeris::EquatorialToHorizontalCalculator::compute(
        {.rightAscensionHours = greenwichRightAscensionHours, .declinationDeg = 0.0},
        {.latitudeDeg = 0.0, .longitudeDeg = -15.0, .elevationMeters = 0.0},
        j2000NoonUtc
    );

    QVERIFY(isNear(eastLongitudeHorizontal.altitudeDeg, 75.0, 1e-9));
    QVERIFY(isNear(westLongitudeHorizontal.altitudeDeg, 75.0, 1e-9));
    QVERIFY(isNear(eastLongitudeHorizontal.azimuthDeg, 270.0, 1e-9));
    QVERIFY(isNear(westLongitudeHorizontal.azimuthDeg, 90.0, 1e-9));
}

void EphemerisRegressionTests::polarObserverPlacesMatchingCelestialPoleAtZenith()
{
    const skygate::core::UtcTimePoint j2000NoonUtc(std::chrono::seconds(946'728'000));

    const auto northPoleHorizontal =
        skygate::ephemeris::EquatorialToHorizontalCalculator::compute(
        {.rightAscensionHours = 12.0, .declinationDeg = 90.0},
        {.latitudeDeg = 90.0, .longitudeDeg = 0.0, .elevationMeters = 0.0},
        j2000NoonUtc
    );
    const auto southPoleHorizontal =
        skygate::ephemeris::EquatorialToHorizontalCalculator::compute(
        {.rightAscensionHours = 12.0, .declinationDeg = -90.0},
        {.latitudeDeg = -90.0, .longitudeDeg = 0.0, .elevationMeters = 0.0},
        j2000NoonUtc
    );

    QVERIFY(isNear(northPoleHorizontal.altitudeDeg, 90.0, 1e-9));
    QVERIFY(isNear(southPoleHorizontal.altitudeDeg, 90.0, 1e-9));
    QVERIFY(std::isfinite(northPoleHorizontal.azimuthDeg));
    QVERIFY(std::isfinite(southPoleHorizontal.azimuthDeg));
}

QTEST_APPLESS_MAIN(EphemerisRegressionTests)

#include "EphemerisRegressionTests.moc"
