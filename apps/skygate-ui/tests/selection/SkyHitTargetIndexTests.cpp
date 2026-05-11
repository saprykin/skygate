#include "SkyHitTargetIndex.hpp"

#include <QtTest/QtTest>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

skygate::ephemeris::CelestialBody makeBody(
    std::string id,
    std::string displayName
)
{
    skygate::ephemeris::CelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    return body;
}

skygate::ephemeris::SkySnapshot makeSnapshot(
    std::vector<skygate::ephemeris::CelestialBody> bodies
)
{
    skygate::ephemeris::SkySnapshot snapshot;
    snapshot.catalogBodies =
        std::make_shared<std::vector<skygate::ephemeris::CelestialBody>>(std::move(bodies));
    return snapshot;
}

}  // namespace

class SkyHitTargetIndexTests final : public QObject {
    Q_OBJECT

private slots:
    void ignoresBodiesWithoutDisplayNames();
    void pointAndGlyphRadiiFollowUiPolicy();
    void invalidViewportAndClearReturnNoHits();
    void overlappingTargetsChooseNearestBody();
};

void SkyHitTargetIndexTests::ignoresBodiesWithoutDisplayNames()
{
    const auto snapshot = makeSnapshot({
        makeBody("hidden", ""),
        makeBody("visible", "Visible"),
    });
    SkyRenderFrame frame;
    frame.points.push_back(SkyRenderPoint {
        .x = 50.0,
        .y = 50.0,
        .sizePx = 20.0,
        .bodyIndex = 0U
    });
    frame.points.push_back(SkyRenderPoint {
        .x = 100.0,
        .y = 100.0,
        .sizePx = 2.0,
        .bodyIndex = 1U
    });

    SkyHitTargetIndex index;
    index.rebuild(frame, snapshot);

    QVERIFY(!index.bodyIndexAt(50.0, 50.0, 200.0, 200.0, snapshot).has_value());
    QCOMPARE(index.bodyIndexAt(100.0, 100.0, 200.0, 200.0, snapshot).value_or(99U), 1U);
}

void SkyHitTargetIndexTests::pointAndGlyphRadiiFollowUiPolicy()
{
    auto snapshot = makeSnapshot({
        makeBody("point", "Point"),
        makeBody("glyph", "Glyph"),
    });
    SkyRenderFrame frame;
    frame.points.push_back(SkyRenderPoint {
        .x = 50.0,
        .y = 50.0,
        .sizePx = 2.0,
        .bodyIndex = 0U
    });
    frame.glyphs.push_back(SkyRenderGlyph {
        .x = 120.0,
        .y = 120.0,
        .radiusXPx = 4.0,
        .radiusYPx = 5.0,
        .bodyIndex = 1U
    });

    SkyHitTargetIndex index;
    index.rebuild(frame, snapshot);

    QCOMPARE(index.bodyIndexAt(60.0, 50.0, 200.0, 200.0, snapshot).value_or(99U), 0U);
    QVERIFY(!index.bodyIndexAt(60.1, 50.0, 200.0, 200.0, snapshot).has_value());
    QCOMPARE(index.bodyIndexAt(132.0, 120.0, 200.0, 200.0, snapshot).value_or(99U), 1U);
    QVERIFY(!index.bodyIndexAt(132.1, 120.0, 200.0, 200.0, snapshot).has_value());
}

void SkyHitTargetIndexTests::invalidViewportAndClearReturnNoHits()
{
    auto snapshot = makeSnapshot({makeBody("target", "Target")});
    SkyRenderFrame frame;
    frame.points.push_back(SkyRenderPoint {
        .x = 50.0,
        .y = 50.0,
        .sizePx = 12.0,
        .bodyIndex = 0U
    });

    SkyHitTargetIndex index;
    index.rebuild(frame, snapshot);

    QVERIFY(!index.bodyIndexAt(50.0, 50.0, 0.0, 200.0, snapshot).has_value());
    QVERIFY(!index.bodyIndexAt(50.0, 50.0, 200.0, -1.0, snapshot).has_value());
    index.clear();
    QVERIFY(!index.bodyIndexAt(50.0, 50.0, 200.0, 200.0, snapshot).has_value());
}

void SkyHitTargetIndexTests::overlappingTargetsChooseNearestBody()
{
    auto snapshot = makeSnapshot({
        makeBody("far", "Far"),
        makeBody("near", "Near"),
    });
    SkyRenderFrame frame;
    frame.points.push_back(SkyRenderPoint {
        .x = 50.0,
        .y = 50.0,
        .sizePx = 30.0,
        .bodyIndex = 0U
    });
    frame.points.push_back(SkyRenderPoint {
        .x = 65.0,
        .y = 50.0,
        .sizePx = 30.0,
        .bodyIndex = 1U
    });

    SkyHitTargetIndex index;
    index.rebuild(frame, snapshot);

    QCOMPARE(index.bodyIndexAt(63.0, 50.0, 200.0, 200.0, snapshot).value_or(99U), 1U);
}

QTEST_APPLESS_MAIN(SkyHitTargetIndexTests)

#include "SkyHitTargetIndexTests.moc"
