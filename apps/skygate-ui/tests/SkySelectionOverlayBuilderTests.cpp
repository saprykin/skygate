#include "SkySelectionOverlayBuilder.hpp"
#include "SkyTimeController.hpp"

#include <QtTest/QtTest>

#include "skygate/core/math/ViewportMath.hpp"
#include "skygate/ephemeris/CatalogFactory.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/IEphemerisEngine.hpp"

#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

skygate::ephemeris::CelestialBody makeBody(
    std::string id,
    std::string displayName,
    const skygate::ephemeris::CelestialBodyType type =
        skygate::ephemeris::CelestialBodyType::Star
)
{
    skygate::ephemeris::CelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.type = type;
    body.visualMagnitude = 2.34;
    body.fixedEquatorial = skygate::core::EquatorialCoordinate {
        .rightAscensionHours = 23.9998,
        .declinationDeg = -12.5
    };
    return body;
}

skygate::ephemeris::CelestialBody makeDeepSkyBody()
{
    auto body = makeBody(
        "messier_031",
        "M31",
        skygate::ephemeris::CelestialBodyType::DeepSkyObject
    );
    body.deepSkyObject = skygate::ephemeris::DeepSkyObjectInfo {
        .kind = skygate::ephemeris::DeepSkyObjectKind::Galaxy,
        .aliases = {"M31", "Andromeda Galaxy", "andromeda galaxy", "NGC 224"},
        .majorAxisArcmin = 190.0,
        .minorAxisArcmin = 60.0
    };
    return body;
}

struct OverlayFixture final {
    skygate::ephemeris::SkySnapshot snapshot;
    QHash<QString, std::size_t> stateIndexByBodyId;
    std::optional<skygate::core::PreparedProjection> projection;
    std::vector<skygate::ephemeris::ConstellationLabelRef> labelRefs;
    std::vector<std::uint8_t> sourceIds;
    QStringList sourceLabels;
    skygate::core::SkyContext skyContext;
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine;
};

OverlayFixture makeFixture()
{
    OverlayFixture fixture;
    auto bodies = std::make_shared<std::vector<skygate::ephemeris::CelestialBody>>();
    bodies->push_back(makeBody("selected", "Selected"));
    bodies->push_back(makeBody("tracked", "Tracked"));
    bodies->push_back(makeBody("search", "Search"));
    bodies->push_back(makeDeepSkyBody());
    bodies->push_back(makeBody("circumpolar", "Circumpolar"));
    bodies->back().fixedEquatorial = skygate::core::EquatorialCoordinate {
        .rightAscensionHours = 4.0,
        .declinationDeg = 80.0
    };
    auto catalog = skygate::ephemeris::createStarCatalogFromBodies(*bodies);
    Q_ASSERT(catalog != nullptr);
    fixture.ephemerisEngine = skygate::ephemeris::createEphemerisEngine(*catalog);
    fixture.snapshot.catalogBodies = bodies;
    fixture.snapshot.states = {
        {
            .bodyIndex = 0U,
            .equatorial = {.rightAscensionHours = 1.0, .declinationDeg = 2.0},
            .horizontal = {.altitudeDeg = 45.0, .azimuthDeg = 180.0}
        },
        {
            .bodyIndex = 1U,
            .equatorial = {.rightAscensionHours = 2.0, .declinationDeg = 3.0},
            .horizontal = {.altitudeDeg = 45.0, .azimuthDeg = 185.0}
        },
        {
            .bodyIndex = 2U,
            .equatorial = {.rightAscensionHours = 3.0, .declinationDeg = 4.0},
            .horizontal = {.altitudeDeg = 45.0, .azimuthDeg = 190.0}
        },
        {
            .bodyIndex = 3U,
            .equatorial = {.rightAscensionHours = 23.9998, .declinationDeg = -12.5},
            .horizontal = {.altitudeDeg = 44.0, .azimuthDeg = 181.0}
        },
        {
            .bodyIndex = 4U,
            .equatorial = {.rightAscensionHours = 4.0, .declinationDeg = 80.0},
            .horizontal = {.altitudeDeg = 50.0, .azimuthDeg = 200.0}
        },
    };
    fixture.stateIndexByBodyId.insert("selected", 0U);
    fixture.stateIndexByBodyId.insert("tracked", 1U);
    fixture.stateIndexByBodyId.insert("search", 2U);
    fixture.stateIndexByBodyId.insert("messier_031", 3U);
    fixture.stateIndexByBodyId.insert("circumpolar", 4U);
    fixture.projection = skygate::core::PreparedProjection::create(
        skygate::core::ProjectionType::Stereographic,
        skygate::core::ViewportMath::buildProjectionParams(1000.0, 800.0, 45.0, 180.0, 90.0)
    );
    fixture.labelRefs = {{"Orion", {"selected", "tracked"}}};
    fixture.sourceIds = {0U, 0U, 0U, 2U, 0U};
    fixture.sourceLabels = {"Catalog", "", "Deep Sky"};
    fixture.skyContext.observer = {
        .latitudeDeg = 47.0,
        .longitudeDeg = 8.0,
        .elevationMeters = 400.0
    };
    fixture.skyContext.utcTime =
        skygate::core::UtcTimePoint(std::chrono::seconds(1'717'276'800));
    return fixture;
}

SkySelectionOverlayInput makeInput(const OverlayFixture& fixture)
{
    return SkySelectionOverlayInput {
        .snapshot = &fixture.snapshot,
        .ephemerisEngine = fixture.ephemerisEngine.get(),
        .preparedProjection = &*fixture.projection,
        .stateIndexByBodyId = &fixture.stateIndexByBodyId,
        .skyContext = fixture.skyContext,
        .constellationLabelRefs = fixture.labelRefs,
        .catalogSourceIds = fixture.sourceIds,
        .catalogSourceLabels = fixture.sourceLabels
    };
}

QString inspectorFieldValue(const QVariantMap& inspector, const QString& label)
{
    const QVariantList fields = inspector.value("fields").toList();
    for (const QVariant& value : fields) {
        const QVariantMap field = value.toMap();
        if (field.value("label").toString() == label) {
            return field.value("value").toString();
        }
    }
    return {};
}

}  // namespace

class SkySelectionOverlayBuilderTests final : public QObject {
    Q_OBJECT

private slots:
    void markerPriorityPrefersSelectedThenTrackedThenSearch();
    void constellationLabelMarkerUsesLabelReferences();
    void inspectorFormatsSourceAliasesAndFallbacks();
    void inspectorIncludesObservationEventsAndFallbacks();
    void pinnedInspectorRendersForUnprojectableBody();
    void activeTrailTargetUsesExpectedPriority();
};

void SkySelectionOverlayBuilderTests::markerPriorityPrefersSelectedThenTrackedThenSearch()
{
    const SkySelectionOverlayBuilder builder;
    const auto fixture = makeFixture();
    auto input = makeInput(fixture);
    input.selectedObjectTargetId = "selected";
    input.trackedTargetKind = "body";
    input.trackedTargetId = "tracked";
    input.selectedSearchTargetKind = "body";
    input.selectedSearchTargetId = "search";

    const QVariantMap selectedMarker = builder.buildSelectionMarker(input);
    QVERIFY(!selectedMarker.isEmpty());

    input.selectedObjectTargetId.clear();
    const QVariantMap trackedMarker = builder.buildSelectionMarker(input);
    QVERIFY(!trackedMarker.isEmpty());
    QVERIFY(trackedMarker.value("x").toDouble() != selectedMarker.value("x").toDouble());

    input.trackedTargetId.clear();
    const QVariantMap searchMarker = builder.buildSelectionMarker(input);
    QVERIFY(!searchMarker.isEmpty());
    QVERIFY(searchMarker.value("x").toDouble() != trackedMarker.value("x").toDouble());
}

void SkySelectionOverlayBuilderTests::constellationLabelMarkerUsesLabelReferences()
{
    const SkySelectionOverlayBuilder builder;
    const auto fixture = makeFixture();
    auto input = makeInput(fixture);
    input.selectedSearchTargetKind = "constellationLabel";
    input.selectedSearchTargetId = "Orion";

    QVERIFY(!builder.buildSelectionMarker(input).isEmpty());

    input.selectedSearchTargetId = "Missing";
    QVERIFY(builder.buildSelectionMarker(input).isEmpty());
}

void SkySelectionOverlayBuilderTests::inspectorFormatsSourceAliasesAndFallbacks()
{
    const SkySelectionOverlayBuilder builder;
    auto fixture = makeFixture();
    auto input = makeInput(fixture);
    input.selectedObjectTargetId = "messier_031";

    const QVariantMap inspector = builder.buildSelectedObjectInspector(input);

    QCOMPARE(inspector.value("title").toString(), QString("M31"));
    QCOMPARE(inspectorFieldValue(inspector, "Type"), QString("Galaxy"));
    QCOMPARE(inspectorFieldValue(inspector, "Source"), QString("Deep Sky"));
    QCOMPARE(inspectorFieldValue(inspector, "Alt / Az"), QString("44.0 / 181.0 deg"));
    QCOMPARE(inspectorFieldValue(inspector, "RA / Dec"), QString("23h 59m 59s / -12d 30m 00s"));
    QVERIFY(inspector.value("aliases").toString().contains("Andromeda Galaxy"));
    QVERIFY(!inspector.value("aliases").toString().contains("M31"));

    fixture.sourceIds = {0U, 0U, 0U, 99U};
    input = makeInput(fixture);
    input.selectedObjectTargetId = "messier_031";
    QCOMPARE(
        inspectorFieldValue(builder.buildSelectedObjectInspector(input), "Source"),
        QString("Catalog")
    );
}

void SkySelectionOverlayBuilderTests::inspectorIncludesObservationEventsAndFallbacks()
{
    const SkySelectionOverlayBuilder builder;
    const auto fixture = makeFixture();
    SkyTimeController timeController;
    QVERIFY(timeController.setTimeZoneId(QStringLiteral("Asia/Bishkek")));
    auto input = makeInput(fixture);
    input.timeController = &timeController;
    input.selectedObjectTargetId = "selected";

    QVariantMap inspector = builder.buildSelectedObjectInspector(input);
    QVERIFY(inspectorFieldValue(inspector, "Rise").contains("UTC+06:00"));
    QVERIFY(inspectorFieldValue(inspector, "Set").contains("UTC+06:00"));
    QVERIFY(inspectorFieldValue(inspector, "Culmination").contains("UTC+06:00"));
    QVERIFY(inspectorFieldValue(inspector, "Culmination").contains("deg"));
    QVERIFY(inspectorFieldValue(inspector, "Next rise").isEmpty());
    QVERIFY(inspectorFieldValue(inspector, "Next set").isEmpty());
    QVERIFY(inspectorFieldValue(inspector, "Max altitude").isEmpty());

    input.selectedObjectTargetId = "circumpolar";
    inspector = builder.buildSelectedObjectInspector(input);
    QCOMPARE(inspectorFieldValue(inspector, "Rise"), QString("Always above"));
    QCOMPARE(inspectorFieldValue(inspector, "Set"), QString("Always above"));
    QVERIFY(inspectorFieldValue(inspector, "Culmination").contains("UTC+06:00"));
    QVERIFY(inspectorFieldValue(inspector, "Culmination").contains("deg"));
}

void SkySelectionOverlayBuilderTests::pinnedInspectorRendersForUnprojectableBody()
{
    const SkySelectionOverlayBuilder builder;
    auto fixture = makeFixture();
    fixture.snapshot.states[0].horizontal = {
        .altitudeDeg = std::numeric_limits<double>::quiet_NaN(),
        .azimuthDeg = std::numeric_limits<double>::quiet_NaN()
    };
    fixture.snapshot.states[0].equatorial = {
        .rightAscensionHours = std::numeric_limits<double>::quiet_NaN(),
        .declinationDeg = std::numeric_limits<double>::quiet_NaN()
    };
    auto input = makeInput(fixture);
    input.selectedObjectTargetId = "selected";

    QVERIFY(builder.buildSelectedObjectInspector(input).isEmpty());

    input.inspectorPinned = true;
    input.inspectorPinnedX = 123.0;
    input.inspectorPinnedY = 456.0;
    const QVariantMap inspector = builder.buildSelectedObjectInspector(input);

    QVERIFY(inspector.value("visible").toBool());
    QCOMPARE(inspector.value("x").toDouble(), 123.0);
    QCOMPARE(inspector.value("y").toDouble(), 456.0);
    QCOMPARE(inspectorFieldValue(inspector, "Alt / Az"), QString("-- / -- deg"));
    QCOMPARE(inspectorFieldValue(inspector, "RA / Dec"), QString("-- / --"));
}

void SkySelectionOverlayBuilderTests::activeTrailTargetUsesExpectedPriority()
{
    const SkySelectionOverlayBuilder builder;
    const auto fixture = makeFixture();
    auto input = makeInput(fixture);
    input.selectedObjectTargetId = "selected";
    input.trackedTargetKind = "body";
    input.trackedTargetId = "tracked";
    input.selectedSearchTargetKind = "body";
    input.selectedSearchTargetId = "search";

    QCOMPARE(builder.activeTrailTargetBodyId(input), QString("selected"));
    QCOMPARE(builder.activeTrailTargetBodyIndex(input).value_or(99U), 0U);

    input.selectedObjectTargetId.clear();
    QCOMPARE(builder.activeTrailTargetBodyId(input), QString("tracked"));
    QCOMPARE(builder.activeTrailTargetBodyIndex(input).value_or(99U), 1U);

    input.trackedTargetId.clear();
    QCOMPARE(builder.activeTrailTargetBodyId(input), QString("search"));
    QCOMPARE(builder.activeTrailTargetBodyIndex(input).value_or(99U), 2U);

    input.selectedSearchTargetId = "missing";
    QVERIFY(!builder.activeTrailTargetBodyIndex(input).has_value());
}

QTEST_APPLESS_MAIN(SkySelectionOverlayBuilderTests)

#include "SkySelectionOverlayBuilderTests.moc"
