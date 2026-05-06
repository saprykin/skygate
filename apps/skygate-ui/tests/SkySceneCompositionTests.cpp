#include "SkySceneComposition.hpp"

#include <QtTest/QtTest>

#include "skygate/core/math/ViewportMath.hpp"

#include <QHash>

#include <optional>

namespace {

std::optional<skygate::core::PreparedProjection> makeProjection()
{
    return skygate::core::PreparedProjection::create(
        skygate::core::ProjectionType::Stereographic,
        skygate::core::ViewportMath::buildProjectionParams(800.0, 600.0, 45.0, 180.0, 80.0)
    );
}

SkyRenderFrame makeFrame(const QString& labelText, const QColor& color)
{
    SkyRenderFrame frame;
    frame.points.push_back(SkyRenderPoint {
        .x = 100.0,
        .y = 120.0,
        .sizePx = 3.0,
        .bodyIndex = 0U,
        .color = color
    });
    frame.labels.push_back(SkyRenderLabel {
        .kind = "body",
        .x = 100.0,
        .y = 120.0,
        .text = labelText,
        .color = color
    });
    return frame;
}

SkySceneCompositionInput makeInput()
{
    SkySceneCompositionInput input;
    input.frameInput.overlayLayers.horizon = false;
    input.frameInput.overlayLayers.altAzGrid = false;
    input.frameInput.overlayLayers.ecliptic = false;
    input.frameInput.overlayLayers.celestialEquator = false;
    input.frameInput.overlayLayers.circumpolarBoundary = false;
    input.viewportWidth = 800.0;
    input.viewportHeight = 600.0;
    return input;
}

SkySceneFramePipelineResult makeFrameResult(
    const bool updated,
    const std::uint64_t renderFrameGeneration,
    const skygate::core::PreparedProjection& projection,
    const skygate::ephemeris::SkySnapshot& snapshot,
    const SkyRenderFrame& frame,
    const QHash<QString, std::size_t>& stateIndexByBodyId
)
{
    return SkySceneFramePipelineResult {
        .updated = updated,
        .snapshotGeneration = 1U,
        .renderFrameGeneration = renderFrameGeneration,
        .preparedProjection = &projection,
        .snapshot = &snapshot,
        .frame = &frame,
        .stateIndexByBodyId = &stateIndexByBodyId
    };
}

}  // namespace

class SkySceneCompositionTests final : public QObject {
    Q_OBJECT

private slots:
    void rebuildCopiesFrameLabelsIntoOverlayItems();
    void unchangedCompositionKeyAvoidsWork();
    void selectionOnlyKeyChangePreservesFrameContent();
    void resetForcesNextCompositionToRebuild();
};

void SkySceneCompositionTests::rebuildCopiesFrameLabelsIntoOverlayItems()
{
    const auto projection = makeProjection();
    QVERIFY(projection.has_value());

    SkySceneComposer composer;
    SkySceneFrameData sceneFrame;
    sceneFrame.preparedProjection = projection;

    const skygate::ephemeris::SkySnapshot snapshot;
    const QHash<QString, std::size_t> stateIndexByBodyId;
    const SkyRenderFrame frame = makeFrame("Vega", QColor("#ffffff"));
    const auto result = composer.rebuild(
        sceneFrame,
        makeInput(),
        makeFrameResult(true, 1U, *projection, snapshot, frame, stateIndexByBodyId)
    );

    QVERIFY(result.changed);
    QVERIFY(result.frameContentChanged);
    QCOMPARE(sceneFrame.frame.points.size(), 1U);
    QCOMPARE(sceneFrame.overlayItems.size(), 1U);
    QCOMPARE(sceneFrame.overlayItems.front().kind, QString("body"));
    QCOMPARE(sceneFrame.overlayItems.front().text, QString("Vega"));
    QCOMPARE(sceneFrame.overlayItems.front().color, QColor("#ffffff"));
}

void SkySceneCompositionTests::unchangedCompositionKeyAvoidsWork()
{
    const auto projection = makeProjection();
    QVERIFY(projection.has_value());

    SkySceneComposer composer;
    SkySceneFrameData sceneFrame;
    sceneFrame.preparedProjection = projection;

    const skygate::ephemeris::SkySnapshot snapshot;
    const QHash<QString, std::size_t> stateIndexByBodyId;
    const SkyRenderFrame frame = makeFrame("Original", QColor("#ffffff"));
    const SkySceneCompositionInput input = makeInput();

    QVERIFY(composer.rebuild(
        sceneFrame,
        input,
        makeFrameResult(true, 7U, *projection, snapshot, frame, stateIndexByBodyId)
    ).changed);

    sceneFrame.overlayItems.clear();
    const auto result = composer.rebuild(
        sceneFrame,
        input,
        makeFrameResult(false, 7U, *projection, snapshot, frame, stateIndexByBodyId)
    );

    QVERIFY(!result.changed);
    QVERIFY(!result.frameContentChanged);
    QVERIFY(sceneFrame.overlayItems.empty());
}

void SkySceneCompositionTests::selectionOnlyKeyChangePreservesFrameContent()
{
    const auto projection = makeProjection();
    QVERIFY(projection.has_value());

    SkySceneComposer composer;
    SkySceneFrameData sceneFrame;
    sceneFrame.preparedProjection = projection;

    const skygate::ephemeris::SkySnapshot snapshot;
    const QHash<QString, std::size_t> stateIndexByBodyId;
    const SkyRenderFrame firstFrame = makeFrame("First", QColor("#ffffff"));
    const SkyRenderFrame replacementFrame = makeFrame("Replacement", QColor("#ff00ff"));
    SkySceneCompositionInput input = makeInput();

    QVERIFY(composer.rebuild(
        sceneFrame,
        input,
        makeFrameResult(true, 3U, *projection, snapshot, firstFrame, stateIndexByBodyId)
    ).changed);

    input.selectedObjectTargetId = "missing-body";
    const auto result = composer.rebuild(
        sceneFrame,
        input,
        makeFrameResult(false, 3U, *projection, snapshot, replacementFrame, stateIndexByBodyId)
    );

    QVERIFY(result.changed);
    QVERIFY(!result.frameContentChanged);
    QCOMPARE(sceneFrame.frame.labels.front().text, QString("First"));
    QCOMPARE(sceneFrame.overlayItems.front().text, QString("First"));
}

void SkySceneCompositionTests::resetForcesNextCompositionToRebuild()
{
    const auto projection = makeProjection();
    QVERIFY(projection.has_value());

    SkySceneComposer composer;
    SkySceneFrameData sceneFrame;
    sceneFrame.preparedProjection = projection;

    const skygate::ephemeris::SkySnapshot snapshot;
    const QHash<QString, std::size_t> stateIndexByBodyId;
    const SkyRenderFrame frame = makeFrame("Reset", QColor("#ffffff"));
    const SkySceneCompositionInput input = makeInput();

    QVERIFY(composer.rebuild(
        sceneFrame,
        input,
        makeFrameResult(true, 11U, *projection, snapshot, frame, stateIndexByBodyId)
    ).changed);

    composer.reset();
    const auto result = composer.rebuild(
        sceneFrame,
        input,
        makeFrameResult(false, 11U, *projection, snapshot, frame, stateIndexByBodyId)
    );

    QVERIFY(result.changed);
    QVERIFY(result.frameContentChanged);
}

QTEST_APPLESS_MAIN(SkySceneCompositionTests)

#include "SkySceneCompositionTests.moc"
