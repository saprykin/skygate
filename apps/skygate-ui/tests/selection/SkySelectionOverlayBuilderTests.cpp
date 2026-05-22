#include "SkySelectionOverlayBuilder.hpp"
#include "SkyOverlayTestSupport.hpp"
#include "SkyTimeController.hpp"
#include "skygate/core/UtcTimeCodec.hpp"

#include <QtTest/QtTest>

#include "skygate/core/math/ViewportMath.hpp"
#include "skygate/ephemeris/CatalogFactory.hpp"
#include "skygate/ephemeris/EphemerisEngineFactory.hpp"
#include "skygate/ephemeris/IEphemerisEngine.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using skygate::ui::tests::overlayInspectorFieldTooltip;
using skygate::ui::tests::overlayInspectorFieldValue;

namespace {

skygate::ephemeris::CelestialBody makeBody(
    std::string id,
    std::string displayName,
    const skygate::ephemeris::CelestialBodyType type = skygate::ephemeris::CelestialBodyType::Star
)
{
    skygate::ephemeris::CelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.type = type;
    body.visualMagnitude = 2.34;
    body.fixedEquatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 23.9998, .declinationDeg = -12.5};
    return body;
}

skygate::ephemeris::CelestialBody makeDeepSkyBody()
{
    auto body = makeBody("messier_031", "M31", skygate::ephemeris::CelestialBodyType::DeepSkyObject);
    body.deepSkyObject = skygate::ephemeris::DeepSkyObjectInfo{
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

class RequestOnlyObservationEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    [[nodiscard]] skygate::ephemeris::SkySnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override
    {
        skygate::ephemeris::SkySnapshot snapshot;
        snapshot.context = request.context;
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::string_view) const override
    {
        return computeBodyState(request, std::size_t{0U});
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::size_t bodyIndex) const override
    {
        if (bodyIndex != 0U) {
            return std::nullopt;
        }

        ++requestSampleCount;
        const bool usesLightTime =
            hasCorrectionFlag(request.options.correctionFlags, skygate::ephemeris::EphemerisCorrectionFlags::LightTime);
        sawLightTimeRequest = sawLightTimeRequest || usesLightTime;
        const double seconds = skygate::core::UtcTimeCodec::secondsSinceEpochDouble(request.context.utcTime);
        const double phase = std::fmod(seconds, 86400.0) / 86400.0;
        return skygate::ephemeris::CelestialBodyState{
            .bodyIndex = 0U,
            .equatorial = {.rightAscensionHours = 0.0, .declinationDeg = 0.0},
            .horizontal = {
                .altitudeDeg = usesLightTime ? 35.0 * std::sin(2.0 * 3.14159265358979323846 * (phase - 0.25)) : -20.0,
                .azimuthDeg = 180.0
            }
        };
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(const skygate::core::SkyContext& context) const override
    {
        skygate::ephemeris::SkySnapshot snapshot;
        snapshot.context = context;
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext&, std::string_view) const override
    {
        ++contextSampleCount;
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext&, std::uint32_t) const override
    {
        ++contextSampleCount;
        return std::nullopt;
    }

    mutable int requestSampleCount = 0;
    mutable int contextSampleCount = 0;
    mutable bool sawLightTimeRequest = false;
};

class NonFixedObservationProfileEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind kind() const noexcept override
    {
        return skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineOptions options() const noexcept override
    {
        skygate::ephemeris::EphemerisEngineOptions options;
        options.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
        options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::LightTime;
        return options;
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override
    {
        skygate::ephemeris::SkySnapshot snapshot;
        snapshot.context = request.context;
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::string_view) const override
    {
        return computeBodyState(request, std::size_t{0U});
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest&, const std::size_t bodyIndex) const override
    {
        if (bodyIndex != 0U) {
            return std::nullopt;
        }

        ++requestSampleCount;
        return skygate::ephemeris::CelestialBodyState{
            .bodyIndex = 0U,
            .equatorial = {.rightAscensionHours = 1.0, .declinationDeg = 2.0},
            .horizontal = {.altitudeDeg = -20.0, .azimuthDeg = 180.0}
        };
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(const skygate::core::SkyContext& context) const override
    {
        skygate::ephemeris::SkySnapshot snapshot;
        snapshot.context = context;
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext&, std::string_view) const override
    {
        ++contextSampleCount;
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext&, std::uint32_t) const override
    {
        ++contextSampleCount;
        return std::nullopt;
    }

    mutable int requestSampleCount = 0;
    mutable int contextSampleCount = 0;
};

class HighPrecisionInspectorEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind kind() const noexcept override
    {
        return skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override
    {
        skygate::ephemeris::SkySnapshot snapshot;
        snapshot.context = request.context;
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::string_view) const override
    {
        return computeBodyState(request, std::size_t{0U});
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::size_t bodyIndex) const override
    {
        if (bodyIndex != 0U) {
            return std::nullopt;
        }

        ++requestSampleCount;
        sawLightTimeRequest =
            sawLightTimeRequest
            || hasCorrectionFlag(
                request.options.correctionFlags, skygate::ephemeris::EphemerisCorrectionFlags::LightTime
            );
        skygate::ephemeris::EphemerisResultMetadata metadata;
        metadata.status = skygate::ephemeris::EphemerisResultStatus::Valid;
        metadata.dataSourceProvenance = "Selected-object precision fixture";
        metadata.appliedCorrections = skygate::ephemeris::EphemerisCorrectionFlags::LightTime;
        metadata.finalizeCorrectionTracking(skygate::ephemeris::EphemerisCorrectionFlags::LightTime);
        return skygate::ephemeris::CelestialBodyState{
            .bodyIndex = 0U,
            .equatorial = {.rightAscensionHours = 6.5, .declinationDeg = 7.5},
            .horizontal = {.altitudeDeg = 12.3, .azimuthDeg = 234.5},
            .metadata = metadata
        };
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(const skygate::core::SkyContext& context) const override
    {
        skygate::ephemeris::SkySnapshot snapshot;
        snapshot.context = context;
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext&, std::string_view) const override
    {
        ++contextSampleCount;
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext&, std::uint32_t) const override
    {
        ++contextSampleCount;
        return std::nullopt;
    }

    mutable int requestSampleCount = 0;
    mutable int contextSampleCount = 0;
    mutable bool sawLightTimeRequest = false;
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
    bodies->back().fixedEquatorial =
        skygate::core::EquatorialCoordinate{.rightAscensionHours = 4.0, .declinationDeg = 80.0};
    auto catalog = skygate::ephemeris::createStarCatalogFromBodies(*bodies);
    Q_ASSERT(catalog != nullptr);
    fixture.ephemerisEngine = skygate::ephemeris::createEphemerisEngine(*catalog);
    fixture.snapshot.catalogBodies = bodies;
    fixture.snapshot.states = {
        {.bodyIndex = 0U,
         .equatorial = {.rightAscensionHours = 1.0, .declinationDeg = 2.0},
         .horizontal = {.altitudeDeg = 45.0, .azimuthDeg = 180.0}},
        {.bodyIndex = 1U,
         .equatorial = {.rightAscensionHours = 2.0, .declinationDeg = 3.0},
         .horizontal = {.altitudeDeg = 45.0, .azimuthDeg = 185.0}},
        {.bodyIndex = 2U,
         .equatorial = {.rightAscensionHours = 3.0, .declinationDeg = 4.0},
         .horizontal = {.altitudeDeg = 45.0, .azimuthDeg = 190.0}},
        {.bodyIndex = 3U,
         .equatorial = {.rightAscensionHours = 23.9998, .declinationDeg = -12.5},
         .horizontal = {.altitudeDeg = 44.0, .azimuthDeg = 181.0}},
        {.bodyIndex = 4U,
         .equatorial = {.rightAscensionHours = 4.0, .declinationDeg = 80.0},
         .horizontal = {.altitudeDeg = 50.0, .azimuthDeg = 200.0}},
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
    fixture.skyContext.observer = {.latitudeDeg = 47.0, .longitudeDeg = 8.0, .elevationMeters = 400.0};
    fixture.skyContext.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1'717'276'800));
    return fixture;
}

SkySelectionOverlayInput makeInput(const OverlayFixture& fixture)
{
    return SkySelectionOverlayInput{
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

}  // namespace

class SkySelectionOverlayBuilderTests final : public QObject {
    Q_OBJECT

private slots:
    void markerPriorityPrefersSelectedThenTrackedThenSearch();
    void constellationLabelMarkerUsesLabelReferences();
    void inspectorFormatsSourceAliasesAndFallbacks();
    void inspectorSurfacesEphemerisMetadataAndWarnings();
    void inspectorUsesHighPrecisionStateForSelectedObject();
    void inspectorIncludesObservationEventsAndFallbacks();
    void inspectorObservationEventsUseRequestOptions();
    void inspectorAvoidsSynchronousHighPrecisionEventSamplingForNonFixedBodies();
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

    const SkySelectionMarker selectedMarker = builder.buildSelectionMarkerData(input);
    QVERIFY(selectedMarker.visible);

    input.selectedObjectTargetId.clear();
    const SkySelectionMarker trackedMarker = builder.buildSelectionMarkerData(input);
    QVERIFY(trackedMarker.visible);
    QVERIFY(trackedMarker.x != selectedMarker.x);

    input.trackedTargetId.clear();
    const SkySelectionMarker searchMarker = builder.buildSelectionMarkerData(input);
    QVERIFY(searchMarker.visible);
    QVERIFY(searchMarker.x != trackedMarker.x);
}

void SkySelectionOverlayBuilderTests::constellationLabelMarkerUsesLabelReferences()
{
    const SkySelectionOverlayBuilder builder;
    const auto fixture = makeFixture();
    auto input = makeInput(fixture);
    input.selectedSearchTargetKind = "constellationLabel";
    input.selectedSearchTargetId = "Orion";

    QVERIFY(builder.buildSelectionMarkerData(input).visible);

    input.selectedSearchTargetId = "Missing";
    QVERIFY(!builder.buildSelectionMarkerData(input).visible);
}

void SkySelectionOverlayBuilderTests::inspectorFormatsSourceAliasesAndFallbacks()
{
    const SkySelectionOverlayBuilder builder;
    auto fixture = makeFixture();
    auto input = makeInput(fixture);
    input.selectedObjectTargetId = "messier_031";

    const SkySelectedObjectInspector inspector = builder.buildSelectedObjectInspectorData(input);

    QCOMPARE(inspector.title, QString("M31"));
    QCOMPARE(overlayInspectorFieldValue(inspector, "Type"), QString("Galaxy"));
    QCOMPARE(overlayInspectorFieldValue(inspector, "Source"), QString("Deep Sky"));
    QCOMPARE(overlayInspectorFieldValue(inspector, "Alt / Az"), QString("44.0 / 181.0 deg"));
    QCOMPARE(overlayInspectorFieldValue(inspector, "RA / Dec"), QString("23h 59m 59s / -12d 30m 00s"));
    QVERIFY(inspector.aliases.contains("Andromeda Galaxy"));
    QVERIFY(!inspector.aliases.contains("M31"));

    fixture.sourceIds = {0U, 0U, 0U, 99U};
    input = makeInput(fixture);
    input.selectedObjectTargetId = "messier_031";
    QCOMPARE(overlayInspectorFieldValue(builder.buildSelectedObjectInspectorData(input), "Source"), QString("Catalog"));
}

void SkySelectionOverlayBuilderTests::inspectorSurfacesEphemerisMetadataAndWarnings()
{
    const SkySelectionOverlayBuilder builder;
    auto fixture = makeFixture();
    auto& metadata = fixture.snapshot.states[0].metadata;
    metadata.status = skygate::ephemeris::EphemerisResultStatus::Degraded;
    metadata.addWarning(skygate::ephemeris::EphemerisWarningCode::MissingEphemerisData);
    metadata.addWarning(skygate::ephemeris::EphemerisWarningCode::DataOutOfRange);
    metadata.dataSourceProvenance = "JPL DE440s smoke fixture";
    metadata.effectiveDataValidityRange = skygate::ephemeris::EphemerisDateRange{
        .displayName = "DE440s short-range kernel",
        .start = *skygate::ephemeris::astronomicalEpochFromCivilDateTime(
            skygate::ephemeris::CivilDateTime{
                .astronomicalYear = 1849,
                .month = 12,
                .day = 26,
            }
        ),
        .end = *skygate::ephemeris::astronomicalEpochFromCivilDateTime(
            skygate::ephemeris::CivilDateTime{
                .astronomicalYear = 2150,
                .month = 1,
                .day = 22,
            }
        )
    };
    metadata.estimatedAngularUncertaintyArcsec = 0.42;
    metadata.appliedCorrections = skygate::ephemeris::EphemerisCorrectionFlags::LightTime;
    metadata.addUnavailableCorrection(skygate::ephemeris::EphemerisCorrectionFlags::AtmosphericRefraction);
    metadata.finalizeCorrectionTracking(
        skygate::ephemeris::EphemerisCorrectionFlags::LightTime
        | skygate::ephemeris::EphemerisCorrectionFlags::AtmosphericRefraction
        | skygate::ephemeris::EphemerisCorrectionFlags::PrecessionNutation
    );

    auto input = makeInput(fixture);
    input.selectedObjectTargetId = "selected";
    input.ephemerisRequest = skygate::ephemeris::EphemerisRequest{
        .epoch = *skygate::ephemeris::astronomicalEpochFromCivilDateTime(skygate::ephemeris::CivilDateTime{}),
        .context = fixture.skyContext,
        .options = skygate::ephemeris::EphemerisEngineOptions{
            .engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision,
        },
    };

    const SkySelectedObjectInspector inspector = builder.buildSelectedObjectInspectorData(input);

    QCOMPARE(inspector.ephemerisStatus, QString("Degraded"));
    QVERIFY(inspector.ephemerisWarningText.contains("Required ephemeris data is unavailable."));
    QVERIFY(inspector.ephemerisWarningText.contains("outside the effective date range"));
    QCOMPARE(overlayInspectorFieldValue(inspector, "Ephemeris"), QString("Degraded"));
    QCOMPARE(overlayInspectorFieldTooltip(inspector, "Ephemeris"), inspector.ephemerisWarningText);
    QCOMPARE(overlayInspectorFieldValue(inspector, "Provenance"), QString("JPL DE440s smoke fixture"));
    QVERIFY(overlayInspectorFieldValue(inspector, "Data range").contains("DE440s short-range kernel"));
    QCOMPARE(overlayInspectorFieldValue(inspector, "Uncertainty"), QString("0.42 arcsec"));
    QVERIFY(overlayInspectorFieldValue(inspector, "Corrections").contains("Applied: light-time"));
    QVERIFY(overlayInspectorFieldValue(inspector, "Corrections").contains("Unavailable: atmospheric refraction"));
    QVERIFY(overlayInspectorFieldValue(inspector, "Corrections").contains("Skipped: precession/nutation"));
}

void SkySelectionOverlayBuilderTests::inspectorUsesHighPrecisionStateForSelectedObject()
{
    const SkySelectionOverlayBuilder builder;
    auto fixture = makeFixture();
    auto engine = std::make_unique<HighPrecisionInspectorEngine>();
    const auto* enginePtr = engine.get();
    fixture.ephemerisEngine = std::move(engine);

    auto input = makeInput(fixture);
    input.selectedObjectTargetId = "selected";
    input.ephemerisRequest = skygate::ephemeris::EphemerisRequest{
        .epoch = *skygate::ephemeris::astronomicalEpochFromCivilDateTime(skygate::ephemeris::CivilDateTime{}),
        .context = fixture.skyContext,
        .options = skygate::ephemeris::EphemerisEngineOptions{
            .engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision,
            .correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::LightTime,
        },
    };

    const SkySelectedObjectInspector inspector = builder.buildSelectedObjectInspectorData(input);

    QCOMPARE(overlayInspectorFieldValue(inspector, "Alt / Az"), QString("12.3 / 234.5 deg"));
    QCOMPARE(overlayInspectorFieldValue(inspector, "RA / Dec"), QString("6h 30m 00s / +7d 30m 00s"));
    QCOMPARE(overlayInspectorFieldValue(inspector, "Provenance"), QString("Selected-object precision fixture"));
    QVERIFY(overlayInspectorFieldValue(inspector, "Corrections").contains("Applied: light-time"));
    QCOMPARE(enginePtr->contextSampleCount, 0);
    QVERIFY(enginePtr->requestSampleCount > 0);
    QVERIFY(enginePtr->sawLightTimeRequest);
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

    SkySelectedObjectInspector inspector = builder.buildSelectedObjectInspectorData(input);
    QVERIFY(overlayInspectorFieldValue(inspector, "Rise").contains("UTC+06:00"));
    QVERIFY(overlayInspectorFieldValue(inspector, "Set").contains("UTC+06:00"));
    QVERIFY(overlayInspectorFieldValue(inspector, "Culmination").contains("UTC+06:00"));
    QVERIFY(overlayInspectorFieldValue(inspector, "Culmination").contains("deg"));
    QVERIFY(overlayInspectorFieldValue(inspector, "Next rise").isEmpty());
    QVERIFY(overlayInspectorFieldValue(inspector, "Next set").isEmpty());
    QVERIFY(overlayInspectorFieldValue(inspector, "Max altitude").isEmpty());

    input.selectedObjectTargetId = "circumpolar";
    inspector = builder.buildSelectedObjectInspectorData(input);
    QCOMPARE(overlayInspectorFieldValue(inspector, "Rise"), QString("Always above"));
    QCOMPARE(overlayInspectorFieldValue(inspector, "Set"), QString("Always above"));
    QVERIFY(overlayInspectorFieldValue(inspector, "Culmination").contains("UTC+06:00"));
    QVERIFY(overlayInspectorFieldValue(inspector, "Culmination").contains("deg"));
}

void SkySelectionOverlayBuilderTests::inspectorObservationEventsUseRequestOptions()
{
    const SkySelectionOverlayBuilder builder;
    auto fixture = makeFixture();
    auto engine = std::make_unique<RequestOnlyObservationEngine>();
    const auto* enginePtr = engine.get();
    fixture.ephemerisEngine = std::move(engine);
    auto mutableBodies =
        std::const_pointer_cast<std::vector<skygate::ephemeris::CelestialBody>>(fixture.snapshot.catalogBodies);
    QVERIFY(mutableBodies != nullptr);
    mutableBodies->at(0).fixedEquatorial.reset();
    fixture.skyContext.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(0));
    auto input = makeInput(fixture);
    input.selectedObjectTargetId = "selected";
    input.ephemerisRequest = skygate::ephemeris::EphemerisRequest{
        .epoch = *skygate::ephemeris::astronomicalEpochFromCivilDateTime(
            skygate::ephemeris::CivilDateTime{
                .astronomicalYear = 1970,
                .month = 1,
                .day = 1,
                .timeScale = skygate::ephemeris::TimeScale::Utc,
            }
        ),
        .context = fixture.skyContext,
        .options = skygate::ephemeris::EphemerisEngineOptions{
            .engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision,
            .correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::NoCorrections,
        },
    };

    SkySelectedObjectInspector inspector = builder.buildSelectedObjectInspectorData(input);
    QCOMPARE(overlayInspectorFieldValue(inspector, "RA / Dec"), QString("1h 00m 00s / +2d 00m 00s"));
    QCOMPARE(overlayInspectorFieldValue(inspector, "Rise"), QString("No event in next 72h"));

    input.ephemerisRequest->options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::LightTime;
    inspector = builder.buildSelectedObjectInspectorData(input);

    QCOMPARE(enginePtr->contextSampleCount, 0);
    QVERIFY(enginePtr->requestSampleCount > 0);
    QVERIFY(overlayInspectorFieldValue(inspector, "Rise").contains("UTC"));
    QVERIFY(overlayInspectorFieldValue(inspector, "Set").contains("UTC"));
    QVERIFY(overlayInspectorFieldValue(inspector, "Culmination").contains("deg"));
}

void SkySelectionOverlayBuilderTests::inspectorAvoidsSynchronousHighPrecisionEventSamplingForNonFixedBodies()
{
    const SkySelectionOverlayBuilder builder;
    auto fixture = makeFixture();
    auto bodies = std::make_shared<std::vector<skygate::ephemeris::CelestialBody>>(*fixture.snapshot.catalogBodies);
    (*bodies)[0].id = "mars";
    (*bodies)[0].displayName = "Mars";
    (*bodies)[0].type = skygate::ephemeris::CelestialBodyType::Planet;
    (*bodies)[0].ephemerisSource = skygate::ephemeris::CelestialBodyEphemerisSource::Planet;
    (*bodies)[0].fixedEquatorial.reset();
    fixture.snapshot.catalogBodies = bodies;
    fixture.stateIndexByBodyId.clear();
    fixture.stateIndexByBodyId.insert(QStringLiteral("mars"), 0U);

    auto engine = std::make_unique<NonFixedObservationProfileEngine>();
    const auto* enginePtr = engine.get();
    fixture.ephemerisEngine = std::move(engine);
    auto input = makeInput(fixture);
    input.selectedObjectTargetId = "mars";
    input.ephemerisRequest = skygate::ephemeris::EphemerisRequest{
        .epoch = *skygate::ephemeris::astronomicalEpochFromCivilDateTime(
            skygate::ephemeris::CivilDateTime{
                .astronomicalYear = 2024,
                .month = 6,
                .day = 1,
                .timeScale = skygate::ephemeris::TimeScale::Utc,
            }
        ),
        .context = fixture.skyContext,
        .options = enginePtr->options(),
    };

    const SkySelectedObjectInspector inspector = builder.buildSelectedObjectInspectorData(input);

    qInfo().noquote() << QStringLiteral(
                             "profile non-fixed inspector selection: highPrecisionRequestSamples=%1 "
                             "highPrecisionContextSamples=%2"
    )
                             .arg(enginePtr->requestSampleCount)
                             .arg(enginePtr->contextSampleCount);
    QCOMPARE(inspector.title, QString("Mars"));
    QCOMPARE(enginePtr->contextSampleCount, 0);
    QVERIFY2(
        enginePtr->requestSampleCount <= 1,
        qPrintable(QStringLiteral("expected only the detailed inspector state lookup, got %1 selected-engine samples")
                       .arg(enginePtr->requestSampleCount))
    );
}

void SkySelectionOverlayBuilderTests::pinnedInspectorRendersForUnprojectableBody()
{
    const SkySelectionOverlayBuilder builder;
    auto fixture = makeFixture();
    fixture.snapshot.states[0].horizontal = {
        .altitudeDeg = std::numeric_limits<double>::quiet_NaN(), .azimuthDeg = std::numeric_limits<double>::quiet_NaN()
    };
    fixture.snapshot.states[0].equatorial = {
        .rightAscensionHours = std::numeric_limits<double>::quiet_NaN(),
        .declinationDeg = std::numeric_limits<double>::quiet_NaN()
    };
    auto input = makeInput(fixture);
    input.selectedObjectTargetId = "selected";

    QVERIFY(!builder.buildSelectedObjectInspectorData(input).visible);

    input.inspectorPinned = true;
    input.inspectorPinnedX = 123.0;
    input.inspectorPinnedY = 456.0;
    const SkySelectedObjectInspector inspector = builder.buildSelectedObjectInspectorData(input);

    QVERIFY(inspector.visible);
    QCOMPARE(inspector.x, 123.0);
    QCOMPARE(inspector.y, 456.0);
    QCOMPARE(overlayInspectorFieldValue(inspector, "Alt / Az"), QString("-- / -- deg"));
    QCOMPARE(overlayInspectorFieldValue(inspector, "RA / Dec"), QString("-- / --"));
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
