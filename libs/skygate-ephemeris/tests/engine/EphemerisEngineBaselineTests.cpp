#include "TestHelpers.hpp"
#include "UtcTimeCodec.hpp"
#include "catalog/CatalogComposer.hpp"
#include "EphemerisEngineFactory.hpp"
#include "catalog/CatalogFactory.hpp"

#include <QtTest/QtTest>

#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
#include <string>

namespace {

constexpr double kSecondsPerDay = 86'400.0;
constexpr double kUnixEpochJulianDay = 2'440'587.5;

skygate::ephemeris::CelestialBody makeBody(
    std::string id,
    std::string displayName,
    const skygate::ephemeris::CelestialBodyType type,
    const double visualMagnitude,
    const std::optional<skygate::core::EquatorialCoordinate>& fixedEquatorial = std::nullopt
)
{
    skygate::ephemeris::CelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.type = type;
    body.visualMagnitude = visualMagnitude;
    body.fixedEquatorial = fixedEquatorial;
    return body;
}

skygate::ephemeris::AstronomicalEpoch epochFromUtc(const skygate::core::UtcTimePoint& utcTime)
{
    const double julianDay =
        skygate::core::UtcTimeCodec::secondsSinceEpochDouble(utcTime) / kSecondsPerDay + kUnixEpochJulianDay;
    const double julianDatePart1 = std::floor(julianDay);
    return {
        .julianDatePart1 = julianDatePart1,
        .julianDatePart2 = julianDay - julianDatePart1,
        .timeScale = skygate::ephemeris::TimeScale::Utc,
    };
}

}  // namespace

class EphemerisEngineBaselineTests final : public QObject {
    Q_OBJECT

private slots:
    void computesFiniteSolarSystemCoordinates();
    void movingBodiesChangeAcrossDays();
    void computesCatalogOwnedReferenceStarCoordinates();
    void supportsNullCatalogAndImportedFixedCoordinates();
    void computesSingleBodyStateByCaseInsensitiveIdAndIndex();
    void requestBasedSnapshotComputeMatchesSkyContextPath();
    void requestBasedSnapshotReportsUnsupportedSimpleOptions();
    void requestWithNoCorrectionsAndAtmosphericRefractionEnabledStaysValid();
    void skyContextCompatibilityPathAppliesEngineDefaultOptions();
    void requestBasedSingleBodyStateByCaseInsensitiveIdAndIndex();
    void requestBasedSingleBodyStateReturnsNulloptForMissingIdAndIndex();
};

void EphemerisEngineBaselineTests::computesFiniteSolarSystemCoordinates()
{
    const auto catalog = skygate::ephemeris::CatalogFactory::createBundledStarCatalog();
    QVERIFY(catalog != nullptr);

    const auto engine = skygate::ephemeris::createEphemerisEngine(*catalog);
    QVERIFY(engine != nullptr);

    skygate::core::SkyContext context;
    context.observer.latitudeDeg = 37.7749;
    context.observer.longitudeDeg = -122.4194;
    context.observer.elevationMeters = 16.0;
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1704067200));

    const auto snapshot = engine->compute(context);
    QVERIFY(snapshot.states.size() == catalog->bodies().size());
    QVERIFY(snapshot.context.observer.latitudeDeg == context.observer.latitudeDeg);

    using namespace skygate::ephemeris::tests;
    const auto* sun = findStateById(snapshot, "sun");
    const auto* moon = findStateById(snapshot, "moon");
    const auto* mercury = findStateById(snapshot, "mercury");
    const auto* venus = findStateById(snapshot, "venus");
    const auto* mars = findStateById(snapshot, "mars");
    const auto* jupiter = findStateById(snapshot, "jupiter");
    const auto* saturn = findStateById(snapshot, "saturn");
    const auto* uranus = findStateById(snapshot, "uranus");
    const auto* neptune = findStateById(snapshot, "neptune");
    const auto* messier31 = findStateById(snapshot, "messier_031");

    QVERIFY(sun != nullptr);
    QVERIFY(sun->equatorial.rightAscensionHours > 0.0 && sun->equatorial.rightAscensionHours < 24.0);
    QVERIFY(std::abs(sun->equatorial.declinationDeg) <= 90.0);
    QVERIFY(std::abs(sun->horizontal.altitudeDeg) <= 90.0);
    QVERIFY(sun->horizontal.azimuthDeg >= 0.0 && sun->horizontal.azimuthDeg < 360.0);

    QVERIFY(moon != nullptr);
    QVERIFY(moon->equatorial.rightAscensionHours > 0.0 && moon->equatorial.rightAscensionHours < 24.0);
    QVERIFY(std::abs(moon->equatorial.declinationDeg) <= 90.0);
    QVERIFY(std::abs(moon->horizontal.altitudeDeg) <= 90.0);
    QVERIFY(moon->horizontal.azimuthDeg >= 0.0 && moon->horizontal.azimuthDeg < 360.0);

    QVERIFY(mercury != nullptr);
    QVERIFY(std::isfinite(mercury->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(mercury->equatorial.declinationDeg));

    QVERIFY(venus != nullptr);
    QVERIFY(std::isfinite(venus->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(venus->equatorial.declinationDeg));

    QVERIFY(mars != nullptr);
    QVERIFY(std::isfinite(mars->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(mars->equatorial.declinationDeg));

    QVERIFY(jupiter != nullptr);
    QVERIFY(std::isfinite(jupiter->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(jupiter->equatorial.declinationDeg));

    QVERIFY(saturn != nullptr);
    QVERIFY(std::isfinite(saturn->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(saturn->equatorial.declinationDeg));

    QVERIFY(uranus != nullptr);
    QVERIFY(std::isfinite(uranus->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(uranus->equatorial.declinationDeg));

    QVERIFY(neptune != nullptr);
    QVERIFY(std::isfinite(neptune->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(neptune->equatorial.declinationDeg));

    QVERIFY(messier31 != nullptr);
    QVERIFY(std::isfinite(messier31->equatorial.rightAscensionHours));
    QVERIFY(std::isfinite(messier31->equatorial.declinationDeg));
    QVERIFY(std::isfinite(messier31->horizontal.altitudeDeg));
    QVERIFY(std::isfinite(messier31->horizontal.azimuthDeg));
}

void EphemerisEngineBaselineTests::movingBodiesChangeAcrossDays()
{
    const auto catalog = skygate::ephemeris::CatalogFactory::createBundledStarCatalog();
    QVERIFY(catalog != nullptr);

    const auto engine = skygate::ephemeris::createEphemerisEngine(*catalog);
    QVERIFY(engine != nullptr);

    skygate::core::SkyContext context;
    context.observer.latitudeDeg = 37.7749;
    context.observer.longitudeDeg = -122.4194;
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1704067200));

    const auto baselineSnapshot = engine->compute(context);
    context.utcTime += std::chrono::seconds(86400);
    const auto nextDaySnapshot = engine->compute(context);

    using namespace skygate::ephemeris::tests;
    const auto* sun = findStateById(baselineSnapshot, "sun");
    const auto* moon = findStateById(baselineSnapshot, "moon");
    const auto* nextSun = findStateById(nextDaySnapshot, "sun");
    const auto* nextMoon = findStateById(nextDaySnapshot, "moon");

    QVERIFY(sun != nullptr);
    QVERIFY(nextSun != nullptr);
    QVERIFY(std::abs(sun->equatorial.rightAscensionHours - nextSun->equatorial.rightAscensionHours) > 1e-5);

    QVERIFY(moon != nullptr);
    QVERIFY(nextMoon != nullptr);
    QVERIFY(std::abs(moon->equatorial.rightAscensionHours - nextMoon->equatorial.rightAscensionHours) > 1e-4);
}

void EphemerisEngineBaselineTests::computesCatalogOwnedReferenceStarCoordinates()
{
    const auto sourceCatalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({
        makeBody("sun", "Sun", skygate::ephemeris::CelestialBodyType::Sun, -26.74),
    });
    QVERIFY(sourceCatalog != nullptr);

    auto activeCatalog = skygate::ephemeris::CatalogComposer::compose({.sourceCatalog = *sourceCatalog});
    QVERIFY(activeCatalog.isSuccess());

    using namespace skygate::ephemeris::tests;
    const auto* siriusBody = findBodyById(activeCatalog.catalog->bodies(), "sirius");
    QVERIFY(siriusBody != nullptr);
    QCOMPARE(siriusBody->ephemerisSource, skygate::ephemeris::CelestialBodyEphemerisSource::FixedEquatorial);

    const auto engine = skygate::ephemeris::createEphemerisEngine(*activeCatalog.catalog);
    QVERIFY(engine != nullptr);

    skygate::core::SkyContext context;
    context.observer.latitudeDeg = 37.7749;
    context.observer.longitudeDeg = -122.4194;
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1704067200));

    const auto state = engine->computeBodyState(context, "SIRIUS");
    QVERIFY(state.has_value());
    QVERIFY(isNear(state->equatorial.rightAscensionHours, 6.7525, 1e-8));
    QVERIFY(isNear(state->equatorial.declinationDeg, -16.7161, 1e-8));
    QVERIFY(std::isfinite(state->horizontal.altitudeDeg));
}

void EphemerisEngineBaselineTests::supportsNullCatalogAndImportedFixedCoordinates()
{
    skygate::core::SkyContext context;
    context.observer.latitudeDeg = 37.7749;
    context.observer.longitudeDeg = -122.4194;
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1704067200));

    const auto nullCatalogEngine = skygate::ephemeris::createEphemerisEngine();
    QVERIFY(nullCatalogEngine != nullptr);
    QVERIFY(nullCatalogEngine->compute(context).states.empty());

    const auto importedCatalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({
        makeBody(
            "demo_star",
            "Demo Star",
            skygate::ephemeris::CelestialBodyType::Star,
            4.0,
            skygate::core::EquatorialCoordinate{.rightAscensionHours = 12.5, .declinationDeg = -30.0}
        ),
    });
    QVERIFY(importedCatalog != nullptr);

    const auto importedEngine = skygate::ephemeris::createEphemerisEngine(*importedCatalog);
    QVERIFY(importedEngine != nullptr);
    const auto importedSnapshot = importedEngine->compute(context);
    QVERIFY(importedSnapshot.states.size() == 1U);
    QVERIFY(skygate::ephemeris::tests::isNear(importedSnapshot.states[0].equatorial.rightAscensionHours, 12.5, 1e-8));
    QVERIFY(skygate::ephemeris::tests::isNear(importedSnapshot.states[0].equatorial.declinationDeg, -30.0, 1e-8));
}

void EphemerisEngineBaselineTests::computesSingleBodyStateByCaseInsensitiveIdAndIndex()
{
    skygate::core::SkyContext context;
    context.observer.latitudeDeg = 37.7749;
    context.observer.longitudeDeg = -122.4194;
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1704067200));

    const auto catalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({
        makeBody(
            "demo_star",
            "Demo Star",
            skygate::ephemeris::CelestialBodyType::Star,
            4.0,
            skygate::core::EquatorialCoordinate{.rightAscensionHours = 12.5, .declinationDeg = -30.0}
        ),
    });
    QVERIFY(catalog != nullptr);

    const auto engine = skygate::ephemeris::createEphemerisEngine(*catalog);
    QVERIFY(engine != nullptr);

    const auto byId = engine->computeBodyState(context, "DEMO_STAR");
    QVERIFY(byId.has_value());
    QCOMPARE(byId->bodyIndex, 0U);
    QVERIFY(std::isfinite(byId->horizontal.altitudeDeg));

    const auto byIndex = engine->computeBodyState(context, 0U);
    QVERIFY(byIndex.has_value());
    QCOMPARE(byIndex->bodyIndex, 0U);
    QCOMPARE(byIndex->equatorial.rightAscensionHours, byId->equatorial.rightAscensionHours);
    QVERIFY(!engine->computeBodyState(context, 1U).has_value());
}

void EphemerisEngineBaselineTests::requestBasedSnapshotComputeMatchesSkyContextPath()
{
    skygate::core::SkyContext context;
    context.observer.latitudeDeg = 37.7749;
    context.observer.longitudeDeg = -122.4194;
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1'704'067'200));

    const auto catalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({
        makeBody(
            "demo_star",
            "Demo Star",
            skygate::ephemeris::CelestialBodyType::Star,
            4.0,
            skygate::core::EquatorialCoordinate{
                .rightAscensionHours = 12.5,
                .declinationDeg = -30.0,
            }
        ),
    });
    QVERIFY(catalog != nullptr);

    const auto engine = skygate::ephemeris::createEphemerisEngine(*catalog);
    QVERIFY(engine != nullptr);

    skygate::ephemeris::EphemerisRequest request;
    request.context = context;
    request.context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(0));
    request.epoch = epochFromUtc(context.utcTime);
    request.options = engine->options();

    const auto contextSnapshot = engine->compute(context);
    const auto requestSnapshot = engine->compute(request);

    QCOMPARE(requestSnapshot.context.utcTime, context.utcTime);
    QCOMPARE(requestSnapshot.context.observer.latitudeDeg, context.observer.latitudeDeg);
    QCOMPARE(requestSnapshot.context.observer.longitudeDeg, context.observer.longitudeDeg);
    QCOMPARE(requestSnapshot.states.size(), contextSnapshot.states.size());

    for (std::size_t stateIndex = 0; stateIndex < contextSnapshot.states.size(); ++stateIndex) {
        const auto& contextState = contextSnapshot.states[stateIndex];
        const auto& requestState = requestSnapshot.states[stateIndex];
        QCOMPARE(requestState.bodyIndex, contextState.bodyIndex);
        QCOMPARE(requestState.metadata.status, contextState.metadata.status);
        QCOMPARE(requestState.metadata.warningCodeMask, contextState.metadata.warningCodeMask);
        QCOMPARE(requestState.metadata.appliedCorrections, contextState.metadata.appliedCorrections);
        QVERIFY(
            skygate::ephemeris::tests::isNear(
                requestState.equatorial.rightAscensionHours, contextState.equatorial.rightAscensionHours, 1e-12
            )
        );
        QVERIFY(
            skygate::ephemeris::tests::isNear(
                requestState.equatorial.declinationDeg, contextState.equatorial.declinationDeg, 1e-12
            )
        );
        QVERIFY(
            skygate::ephemeris::tests::isNear(
                requestState.horizontal.altitudeDeg, contextState.horizontal.altitudeDeg, 1e-12
            )
        );
        QVERIFY(
            skygate::ephemeris::tests::isNear(
                requestState.horizontal.azimuthDeg, contextState.horizontal.azimuthDeg, 1e-12
            )
        );
    }
}

void EphemerisEngineBaselineTests::requestBasedSnapshotReportsUnsupportedSimpleOptions()
{
    skygate::core::SkyContext context;
    context.observer.latitudeDeg = 37.7749;
    context.observer.longitudeDeg = -122.4194;
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1'704'067'200));

    const auto catalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({
        makeBody(
            "demo_star",
            "Demo Star",
            skygate::ephemeris::CelestialBodyType::Star,
            4.0,
            skygate::core::EquatorialCoordinate{
                .rightAscensionHours = 12.5,
                .declinationDeg = -30.0,
            }
        ),
    });
    QVERIFY(catalog != nullptr);

    const auto engine = skygate::ephemeris::createEphemerisEngine(*catalog);
    QVERIFY(engine != nullptr);

    skygate::ephemeris::EphemerisRequest request;
    request.context = context;
    request.epoch = epochFromUtc(context.utcTime);
    request.options.engineKind = skygate::ephemeris::EphemerisEngineKind::Simple;
    request.options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::LightTime
                                      | skygate::ephemeris::EphemerisCorrectionFlags::AtmosphericRefraction;
    request.options.enableAtmosphericRefraction = true;

    const auto contextSnapshot = engine->compute(context);
    const auto requestSnapshot = engine->compute(request);

    QCOMPARE(requestSnapshot.states.size(), contextSnapshot.states.size());
    QCOMPARE(requestSnapshot.states.size(), 1U);

    const auto& contextState = contextSnapshot.states.front();
    const auto& requestState = requestSnapshot.states.front();
    QCOMPARE(requestState.metadata.status, skygate::ephemeris::EphemerisResultStatus::Degraded);
    QVERIFY(requestState.metadata.hasWarning(skygate::ephemeris::EphemerisWarningCode::CorrectionUnavailable));
    QCOMPARE(requestState.metadata.appliedCorrections, skygate::ephemeris::EphemerisCorrectionFlags::NoCorrections);
    QVERIFY(
        skygate::ephemeris::tests::isNear(
            requestState.equatorial.rightAscensionHours, contextState.equatorial.rightAscensionHours, 1e-12
        )
    );
    QVERIFY(
        skygate::ephemeris::tests::isNear(
            requestState.equatorial.declinationDeg, contextState.equatorial.declinationDeg, 1e-12
        )
    );
    QVERIFY(
        skygate::ephemeris::tests::isNear(
            requestState.horizontal.altitudeDeg, contextState.horizontal.altitudeDeg, 1e-12
        )
    );
    QVERIFY(
        skygate::ephemeris::tests::isNear(requestState.horizontal.azimuthDeg, contextState.horizontal.azimuthDeg, 1e-12)
    );
}

void EphemerisEngineBaselineTests::requestWithNoCorrectionsAndAtmosphericRefractionEnabledStaysValid()
{
    skygate::core::SkyContext context;
    context.observer.latitudeDeg = 37.7749;
    context.observer.longitudeDeg = -122.4194;
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1'704'067'200));

    const auto catalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({
        makeBody(
            "demo_star",
            "Demo Star",
            skygate::ephemeris::CelestialBodyType::Star,
            4.0,
            skygate::core::EquatorialCoordinate{
                .rightAscensionHours = 12.5,
                .declinationDeg = -30.0,
            }
        ),
    });
    QVERIFY(catalog != nullptr);

    const auto engine = skygate::ephemeris::createEphemerisEngine(*catalog);
    QVERIFY(engine != nullptr);

    skygate::ephemeris::EphemerisRequest request;
    request.context = context;
    request.epoch = epochFromUtc(context.utcTime);
    request.options.engineKind = skygate::ephemeris::EphemerisEngineKind::Simple;
    request.options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::NoCorrections;
    request.options.enableAtmosphericRefraction = true;

    const auto snapshot = engine->compute(request);

    QCOMPARE(snapshot.states.size(), 1U);
    const auto& state = snapshot.states.front();
    QCOMPARE(state.metadata.status, skygate::ephemeris::EphemerisResultStatus::Valid);
    QVERIFY(!state.metadata.hasWarning(skygate::ephemeris::EphemerisWarningCode::CorrectionUnavailable));
    QCOMPARE(state.metadata.requestedCorrections, skygate::ephemeris::EphemerisCorrectionFlags::NoCorrections);
    QCOMPARE(state.metadata.appliedCorrections, skygate::ephemeris::EphemerisCorrectionFlags::NoCorrections);
    QCOMPARE(state.metadata.skippedCorrections, skygate::ephemeris::EphemerisCorrectionFlags::NoCorrections);
    QCOMPARE(state.metadata.unavailableCorrections, skygate::ephemeris::EphemerisCorrectionFlags::NoCorrections);
}

void EphemerisEngineBaselineTests::skyContextCompatibilityPathAppliesEngineDefaultOptions()
{
    skygate::core::SkyContext context;
    context.observer.latitudeDeg = 37.7749;
    context.observer.longitudeDeg = -122.4194;
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1'704'067'200));

    const auto catalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({
        makeBody(
            "demo_star",
            "Demo Star",
            skygate::ephemeris::CelestialBodyType::Star,
            4.0,
            skygate::core::EquatorialCoordinate{
                .rightAscensionHours = 12.5,
                .declinationDeg = -30.0,
            }
        ),
    });
    QVERIFY(catalog != nullptr);

    const auto engine = skygate::ephemeris::createEphemerisEngine(*catalog);
    QVERIFY(engine != nullptr);

    skygate::ephemeris::EphemerisRequest defaultRequest;
    defaultRequest.context = context;
    defaultRequest.epoch = epochFromUtc(context.utcTime);

    const auto contextSnapshot = engine->compute(context);
    const auto requestSnapshot = engine->compute(defaultRequest);
    QCOMPARE(contextSnapshot.states.size(), 1U);
    QCOMPARE(requestSnapshot.states.size(), 1U);

    const auto& contextState = contextSnapshot.states.front();
    const auto& requestState = requestSnapshot.states.front();
    QCOMPARE(contextState.metadata.status, skygate::ephemeris::EphemerisResultStatus::Valid);
    QVERIFY(!contextState.metadata.hasWarning(skygate::ephemeris::EphemerisWarningCode::CorrectionUnavailable));
    QCOMPARE(contextState.metadata.appliedCorrections, skygate::ephemeris::EphemerisCorrectionFlags::NoCorrections);
    QCOMPARE(requestState.metadata.status, skygate::ephemeris::EphemerisResultStatus::Degraded);
    QVERIFY(requestState.metadata.hasWarning(skygate::ephemeris::EphemerisWarningCode::CorrectionUnavailable));

    const auto contextBodyState = engine->computeBodyState(context, "demo_star");
    QVERIFY(contextBodyState.has_value());
    QCOMPARE(contextBodyState->metadata.status, skygate::ephemeris::EphemerisResultStatus::Valid);
    QVERIFY(!contextBodyState->metadata.hasWarning(skygate::ephemeris::EphemerisWarningCode::CorrectionUnavailable));

    const auto requestBodyState = engine->computeBodyState(defaultRequest, "demo_star");
    QVERIFY(requestBodyState.has_value());
    QCOMPARE(requestBodyState->metadata.status, skygate::ephemeris::EphemerisResultStatus::Degraded);
    QVERIFY(requestBodyState->metadata.hasWarning(skygate::ephemeris::EphemerisWarningCode::CorrectionUnavailable));
}

void EphemerisEngineBaselineTests::requestBasedSingleBodyStateByCaseInsensitiveIdAndIndex()
{
    skygate::core::SkyContext context;
    context.observer.latitudeDeg = 37.7749;
    context.observer.longitudeDeg = -122.4194;
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1'704'067'200));

    const auto catalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({
        makeBody(
            "demo_star",
            "Demo Star",
            skygate::ephemeris::CelestialBodyType::Star,
            4.0,
            skygate::core::EquatorialCoordinate{
                .rightAscensionHours = 12.5,
                .declinationDeg = -30.0,
            }
        ),
    });
    QVERIFY(catalog != nullptr);

    const auto engine = skygate::ephemeris::createEphemerisEngine(*catalog);
    QVERIFY(engine != nullptr);

    skygate::ephemeris::EphemerisRequest request;
    request.context = context;
    request.context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(0));
    request.epoch = epochFromUtc(context.utcTime);
    request.options = engine->options();

    const auto contextState = engine->computeBodyState(context, "demo_star");
    QVERIFY(contextState.has_value());

    const auto byId = engine->computeBodyState(request, "DEMO_STAR");
    QVERIFY(byId.has_value());
    QCOMPARE(byId->bodyIndex, 0U);
    QCOMPARE(byId->metadata.status, contextState->metadata.status);
    QVERIFY(
        skygate::ephemeris::tests::isNear(
            byId->equatorial.rightAscensionHours, contextState->equatorial.rightAscensionHours, 1e-12
        )
    );
    QVERIFY(
        skygate::ephemeris::tests::isNear(
            byId->equatorial.declinationDeg, contextState->equatorial.declinationDeg, 1e-12
        )
    );
    QVERIFY(
        skygate::ephemeris::tests::isNear(byId->horizontal.altitudeDeg, contextState->horizontal.altitudeDeg, 1e-12)
    );
    QVERIFY(skygate::ephemeris::tests::isNear(byId->horizontal.azimuthDeg, contextState->horizontal.azimuthDeg, 1e-12));

    const auto byIndex = engine->computeBodyState(request, std::size_t{0});
    QVERIFY(byIndex.has_value());
    QCOMPARE(byIndex->bodyIndex, 0U);
    QCOMPARE(byIndex->metadata.status, byId->metadata.status);
    QVERIFY(
        skygate::ephemeris::tests::isNear(
            byIndex->equatorial.rightAscensionHours, byId->equatorial.rightAscensionHours, 1e-12
        )
    );
    QVERIFY(
        skygate::ephemeris::tests::isNear(byIndex->equatorial.declinationDeg, byId->equatorial.declinationDeg, 1e-12)
    );
    QVERIFY(skygate::ephemeris::tests::isNear(byIndex->horizontal.altitudeDeg, byId->horizontal.altitudeDeg, 1e-12));
    QVERIFY(skygate::ephemeris::tests::isNear(byIndex->horizontal.azimuthDeg, byId->horizontal.azimuthDeg, 1e-12));
}

void EphemerisEngineBaselineTests::requestBasedSingleBodyStateReturnsNulloptForMissingIdAndIndex()
{
    skygate::core::SkyContext context;
    context.observer.latitudeDeg = 37.7749;
    context.observer.longitudeDeg = -122.4194;
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1'704'067'200));

    const auto catalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({
        makeBody(
            "demo_star",
            "Demo Star",
            skygate::ephemeris::CelestialBodyType::Star,
            4.0,
            skygate::core::EquatorialCoordinate{
                .rightAscensionHours = 12.5,
                .declinationDeg = -30.0,
            }
        ),
    });
    QVERIFY(catalog != nullptr);

    const auto engine = skygate::ephemeris::createEphemerisEngine(*catalog);
    QVERIFY(engine != nullptr);

    skygate::ephemeris::EphemerisRequest request;
    request.context = context;
    request.epoch = epochFromUtc(context.utcTime);
    request.options = engine->options();

    QVERIFY(!engine->computeBodyState(request, "missing").has_value());
    QVERIFY(!engine->computeBodyState(request, "").has_value());
    QVERIFY(!engine->computeBodyState(request, std::size_t{1}).has_value());
}

QTEST_APPLESS_MAIN(EphemerisEngineBaselineTests)

#include "EphemerisEngineBaselineTests.moc"
