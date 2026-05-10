#include "SkyRenderBuilders.hpp"

#include <QtTest/QtTest>

#include "skygate/core/math/ViewportMath.hpp"
#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

skygate::ephemeris::CelestialBody makeBody(
    std::string id,
    std::string displayName,
    const skygate::ephemeris::CelestialBodyType type,
    const double magnitude
)
{
    skygate::ephemeris::CelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.type = type;
    body.visualMagnitude = magnitude;
    return body;
}

skygate::ephemeris::CelestialBody makeDeepSkyBody(
    std::string id,
    std::string displayName,
    const double magnitude,
    const skygate::ephemeris::DeepSkyObjectKind kind =
        skygate::ephemeris::DeepSkyObjectKind::Galaxy,
    std::vector<std::string> aliases = {"Common Name"}
)
{
    auto body = makeBody(
        std::move(id),
        std::move(displayName),
        skygate::ephemeris::CelestialBodyType::DeepSkyObject,
        magnitude
    );
    body.deepSkyObject = skygate::ephemeris::DeepSkyObjectInfo {
        .kind = kind,
        .aliases = std::move(aliases),
        .majorAxisArcmin = 120.0,
        .minorAxisArcmin = 30.0,
        .positionAngleDeg = 42.0
    };
    return body;
}

struct FrameFixture final {
    skygate::ephemeris::SkySnapshot snapshot;
    std::optional<skygate::core::PreparedProjection> projection;
    skygate::ui::internal::SkyThemeRenderPalette renderTheme;
};

FrameFixture makeFixture(
    std::vector<skygate::ephemeris::CelestialBody> bodies,
    const double fovDeg = 40.0
)
{
    FrameFixture fixture;
    auto catalogBodies =
        std::make_shared<std::vector<skygate::ephemeris::CelestialBody>>(std::move(bodies));
    fixture.snapshot.catalogBodies = catalogBodies;
    for (std::uint32_t index = 0; index < catalogBodies->size(); ++index) {
        fixture.snapshot.states.push_back(skygate::ephemeris::CelestialBodyState {
            .bodyIndex = index,
            .horizontal = {
                .altitudeDeg = 45.0 + (static_cast<double>(index % 5U) * 0.01),
                .azimuthDeg = 180.0 + (static_cast<double>(index % 5U) * 0.01)
            }
        });
    }

    fixture.projection = skygate::core::PreparedProjection::create(
        skygate::core::ProjectionType::Stereographic,
        skygate::core::ViewportMath::buildProjectionParams(1000.0, 800.0, 45.0, 180.0, fovDeg)
    );
    fixture.renderTheme.bodyStar = QColor("#ffffff");
    fixture.renderTheme.bodyDeepSkyObject = QColor("#00ffff");
    fixture.renderTheme.bodyPlanet = QColor("#ffcc66");
    fixture.renderTheme.bodyConstellation = QColor("#8888ff");
    fixture.renderTheme.labelDeepSkyObject = QColor("#00ffff");
    fixture.renderTheme.labelPlanet = QColor("#ffcc66");
    fixture.renderTheme.labelConstellation = QColor("#8888ff");
    fixture.renderTheme.labelDefault = QColor("#ffffff");
    fixture.renderTheme.constellationLine = QColor("#4455ff");
    return fixture;
}

SkyRenderFrame buildFrame(
    const FrameFixture& fixture,
    const SkyOverlayLayerVisibility& overlayLayers = {},
    const std::span<const skygate::ephemeris::ConstellationLineRef> lineRefs = {},
    const std::span<const skygate::ephemeris::ConstellationLabelRef> labelRefs = {},
    const double magnitudeCutoff = 8.0
)
{
    const SkyRenderFrameBuilder builder;
    return builder.buildFrame(
        fixture.snapshot,
        *fixture.projection,
        lineRefs,
        labelRefs,
        magnitudeCutoff,
        1000.0,
        800.0,
        fixture.renderTheme,
        overlayLayers
    );
}

bool labelsContainText(const std::vector<SkyRenderLabel>& labels, const QString& text)
{
    for (const SkyRenderLabel& label : labels) {
        if (label.text == text) {
            return true;
        }
    }
    return false;
}

}  // namespace

class SkyRenderBuildersTests final : public QObject {
    Q_OBJECT

private slots:
    void starDecimationKeepsBrighterStarInScreenCell();
    void deepSkyVisibilityAndGlyphPolicyFollowPresentationRules();
    void deepSkyGlyphRendersWhenCenterIsJustOutsideViewport();
    void deepSkyLabelsPreferNamedObjectsAndRespectVisibility();
    void constellationLinesAndLabelsRespectRefsAndVisibility();
    void constellationLineRendersWhenEndpointsAreOutsideViewport();
};

void SkyRenderBuildersTests::starDecimationKeepsBrighterStarInScreenCell()
{
    std::vector<skygate::ephemeris::CelestialBody> bodies;
    bodies.reserve(30001U);
    bodies.push_back(makeBody("faint", "Faint", skygate::ephemeris::CelestialBodyType::Star, 5.0));
    bodies.push_back(makeBody("bright", "Bright", skygate::ephemeris::CelestialBodyType::Star, 1.0));
    for (int index = 2; index < 30001; ++index) {
        bodies.push_back(makeBody(
            "star_" + std::to_string(index),
            "Star",
            skygate::ephemeris::CelestialBodyType::Star,
            6.0
        ));
    }

    auto fixture = makeFixture(std::move(bodies), 120.0);
    for (auto& state : fixture.snapshot.states) {
        state.horizontal = {.altitudeDeg = 45.0, .azimuthDeg = 180.0};
    }

    const auto frame = buildFrame(fixture);

    QCOMPARE(frame.points.size(), 1U);
    QCOMPARE(frame.points.front().bodyIndex, 1U);
}

void SkyRenderBuildersTests::deepSkyVisibilityAndGlyphPolicyFollowPresentationRules()
{
    auto fixture = makeFixture({
        makeDeepSkyBody("bright", "Bright DSO", 7.0),
        makeDeepSkyBody("faint", "Faint DSO", 8.5, skygate::ephemeris::DeepSkyObjectKind::PlanetaryNebula),
    }, 80.0);

    auto frame = buildFrame(fixture);
    QCOMPARE(frame.glyphs.size(), 1U);
    QCOMPARE(frame.glyphs.front().bodyIndex, 0U);
    QCOMPARE(frame.glyphs.front().rotationDeg, 42.0);

    fixture = makeFixture({
        makeDeepSkyBody("faint", "Faint DSO", 8.5, skygate::ephemeris::DeepSkyObjectKind::PlanetaryNebula),
    }, 20.0);
    frame = buildFrame(fixture);
    QCOMPARE(frame.glyphs.size(), 1U);
    QCOMPARE(frame.glyphs.front().kind, skygate::ephemeris::DeepSkyObjectKind::PlanetaryNebula);
    QCOMPARE(frame.glyphs.front().radiusXPx, frame.glyphs.front().radiusYPx);

    SkyOverlayLayerVisibility hidden;
    hidden.deepSkyObjects = false;
    frame = buildFrame(fixture, hidden);
    QVERIFY(frame.glyphs.empty());
}

void SkyRenderBuildersTests::deepSkyGlyphRendersWhenCenterIsJustOutsideViewport()
{
    auto fixture = makeFixture({
        makeDeepSkyBody("edge", "Edge DSO", 7.0),
    }, 1.0);
    fixture.snapshot.states.front().horizontal = {.altitudeDeg = 45.0, .azimuthDeg = 180.76};

    QVERIFY(!fixture.projection->project(fixture.snapshot.states.front().horizontal).isVisible);

    const auto frame = buildFrame(fixture);

    QCOMPARE(frame.glyphs.size(), 1U);
}

void SkyRenderBuildersTests::deepSkyLabelsPreferNamedObjectsAndRespectVisibility()
{
    auto fixture = makeFixture({
        makeDeepSkyBody(
            "ngc_100",
            "NGC 100",
            8.0,
            skygate::ephemeris::DeepSkyObjectKind::Galaxy,
            {"NGC 100"}
        ),
        makeDeepSkyBody("messier_031", "M31", 8.0),
    }, 40.0);

    auto frame = buildFrame(fixture);

    QVERIFY(labelsContainText(frame.labels, "M31"));
    QVERIFY(!labelsContainText(frame.labels, "NGC 100"));

    SkyOverlayLayerVisibility hidden;
    hidden.deepSkyLabels = false;
    frame = buildFrame(fixture, hidden);
    QVERIFY(!labelsContainText(frame.labels, "M31"));
}

void SkyRenderBuildersTests::constellationLinesAndLabelsRespectRefsAndVisibility()
{
    auto fixture = makeFixture({
        makeBody("hip_a", "", skygate::ephemeris::CelestialBodyType::Constellation, 1.0),
        makeBody("hip_b", "", skygate::ephemeris::CelestialBodyType::Constellation, 1.0),
    });
    const std::vector<skygate::ephemeris::ConstellationLineRef> lineRefs {
        {"hip_a", "hip_b"},
        {"hip_a", "missing"},
    };
    const std::vector<skygate::ephemeris::ConstellationLabelRef> labelRefs {
        {"Demo", {"hip_a", "hip_b"}},
        {"Missing", {"missing"}},
    };

    auto frame = buildFrame(fixture, {}, lineRefs, labelRefs);

    QCOMPARE(frame.lines.size(), 1U);
    QVERIFY(labelsContainText(frame.labels, "Demo"));
    QVERIFY(!labelsContainText(frame.labels, "Missing"));

    SkyOverlayLayerVisibility hidden;
    hidden.constellationLines = false;
    hidden.constellationLabels = false;
    frame = buildFrame(fixture, hidden, lineRefs, labelRefs);
    QVERIFY(frame.lines.empty());
    QVERIFY(!labelsContainText(frame.labels, "Demo"));
}

void SkyRenderBuildersTests::constellationLineRendersWhenEndpointsAreOutsideViewport()
{
    auto fixture = makeFixture({
        makeBody("hip_a", "", skygate::ephemeris::CelestialBodyType::Constellation, 1.0),
        makeBody("hip_b", "", skygate::ephemeris::CelestialBodyType::Constellation, 1.0),
    }, 1.0);
    fixture.snapshot.states[0].horizontal = {.altitudeDeg = 45.0, .azimuthDeg = 179.0};
    fixture.snapshot.states[1].horizontal = {.altitudeDeg = 45.0, .azimuthDeg = 181.0};
    const std::vector<skygate::ephemeris::ConstellationLineRef> lineRefs {
        {"hip_a", "hip_b"},
    };

    QVERIFY(!fixture.projection->project(fixture.snapshot.states[0].horizontal).isVisible);
    QVERIFY(!fixture.projection->project(fixture.snapshot.states[1].horizontal).isVisible);

    const auto frame = buildFrame(fixture, {}, lineRefs);

    QVERIFY(!frame.lines.empty());
}

QTEST_APPLESS_MAIN(SkyRenderBuildersTests)

#include "SkyRenderBuildersTests.moc"
