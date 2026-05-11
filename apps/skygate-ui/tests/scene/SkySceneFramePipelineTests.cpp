#include "SkySceneFramePipeline.hpp"

#include <QtTest/QtTest>

#include "skygate/ephemeris/EphemerisEngineQueries.hpp"
#include "skygate/ephemeris/IEphemerisEngine.hpp"

#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

class CountingEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    explicit CountingEngine(std::string bodyId = "target")
        : m_bodyId(std::move(bodyId))
    {
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(
        const skygate::core::SkyContext& context
    ) const override
    {
        ++m_computeCount;
        skygate::ephemeris::CelestialBody body;
        body.id = m_bodyId;
        body.displayName = "Target";
        body.visualMagnitude = 1.0;

        skygate::ephemeris::SkySnapshot snapshot;
        snapshot.context = context;
        auto bodies = std::make_shared<std::vector<skygate::ephemeris::CelestialBody>>();
        bodies->push_back(std::move(body));
        snapshot.catalogBodies = std::move(bodies);
        snapshot.states.push_back(skygate::ephemeris::CelestialBodyState {
            .bodyIndex = 0U,
            .horizontal = {.altitudeDeg = 45.0, .azimuthDeg = 180.0}
        });
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState> computeBodyState(
        const skygate::core::SkyContext& context,
        const std::string_view bodyId
    ) const override
    {
        return skygate::ephemeris::EphemerisEngineQueries::computeBodyStateById(
            *this,
            context,
            bodyId
        );
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState> computeBodyState(
        const skygate::core::SkyContext& context,
        const std::uint32_t bodyIndex
    ) const override
    {
        return skygate::ephemeris::EphemerisEngineQueries::computeBodyStateByIndex(
            *this,
            context,
            bodyIndex
        );
    }

    [[nodiscard]] int computeCount() const noexcept
    {
        return m_computeCount;
    }

private:
    std::string m_bodyId;
    mutable int m_computeCount = 0;
};

SkySceneFramePipelineInput makeInput(const CountingEngine& engine)
{
    SkySceneFramePipelineInput input;
    input.ephemerisEngine = &engine;
    input.skyContext.observer = {
        .latitudeDeg = 47.0,
        .longitudeDeg = 8.0,
        .elevationMeters = 400.0
    };
    input.catalogRevision = 1U;
    input.projectionType = skygate::core::ProjectionType::Stereographic;
    input.viewCenterAltitudeDeg = 45.0;
    input.viewCenterAzimuthDeg = 180.0;
    input.viewFieldOfViewDeg = 90.0;
    input.magnitudeCutoff = 6.0;
    input.themeId = "default";
    return input;
}

}  // namespace

class SkySceneFramePipelineTests final : public QObject {
    Q_OBJECT

private slots:
    void rejectsInvalidInputs();
    void repeatedKeysReportNoUpdate();
    void renderOnlyChangesAvoidSnapshotRecompute();
    void snapshotKeyChangesRecomputeSnapshot();
    void clearReportsWhetherStateWasPresent();
};

void SkySceneFramePipelineTests::rejectsInvalidInputs()
{
    CountingEngine engine;
    SkySceneFramePipeline pipeline;
    auto input = makeInput(engine);

    input.ephemerisEngine = nullptr;
    QVERIFY(!pipeline.rebuild(input, 1000.0, 800.0).has_value());

    input = makeInput(engine);
    QVERIFY(!pipeline.rebuild(input, 0.0, 800.0).has_value());

    input = makeInput(engine);
    input.viewCenterAltitudeDeg = std::numeric_limits<double>::quiet_NaN();
    QVERIFY(!pipeline.rebuild(input, 1000.0, 800.0).has_value());
}

void SkySceneFramePipelineTests::repeatedKeysReportNoUpdate()
{
    CountingEngine engine;
    SkySceneFramePipeline pipeline;
    const auto input = makeInput(engine);

    const auto first = pipeline.rebuild(input, 1000.0, 800.0);
    QVERIFY(first.has_value());
    QVERIFY(first->updated);
    QCOMPARE(first->snapshotGeneration, 1U);
    QCOMPARE(first->renderFrameGeneration, 1U);
    QCOMPARE(engine.computeCount(), 1);

    const auto second = pipeline.rebuild(input, 1000.0, 800.0);
    QVERIFY(second.has_value());
    QVERIFY(!second->updated);
    QCOMPARE(second->snapshotGeneration, 1U);
    QCOMPARE(second->renderFrameGeneration, 1U);
    QCOMPARE(engine.computeCount(), 1);
}

void SkySceneFramePipelineTests::renderOnlyChangesAvoidSnapshotRecompute()
{
    CountingEngine engine;
    SkySceneFramePipeline pipeline;
    auto input = makeInput(engine);
    QVERIFY(pipeline.rebuild(input, 1000.0, 800.0).has_value());

    input.viewCenterAzimuthDeg = 190.0;
    auto result = pipeline.rebuild(input, 1000.0, 800.0);
    QVERIFY(result.has_value());
    QVERIFY(result->updated);
    QCOMPARE(result->snapshotGeneration, 1U);
    QCOMPARE(result->renderFrameGeneration, 2U);
    QCOMPARE(engine.computeCount(), 1);

    input.themeId = "night";
    result = pipeline.rebuild(input, 1000.0, 800.0);
    QVERIFY(result.has_value());
    QCOMPARE(result->snapshotGeneration, 1U);
    QCOMPARE(result->renderFrameGeneration, 3U);

    input.overlayLayers.horizon = false;
    result = pipeline.rebuild(input, 1000.0, 800.0);
    QVERIFY(result.has_value());
    QCOMPARE(result->snapshotGeneration, 1U);
    QCOMPARE(result->renderFrameGeneration, 4U);
}

void SkySceneFramePipelineTests::snapshotKeyChangesRecomputeSnapshot()
{
    CountingEngine engine;
    CountingEngine otherEngine("other");
    SkySceneFramePipeline pipeline;
    auto input = makeInput(engine);
    QVERIFY(pipeline.rebuild(input, 1000.0, 800.0).has_value());

    input.catalogRevision = 2U;
    auto result = pipeline.rebuild(input, 1000.0, 800.0);
    QVERIFY(result.has_value());
    QCOMPARE(result->snapshotGeneration, 2U);
    QCOMPARE(engine.computeCount(), 2);

    input.skyContext.observer.latitudeDeg = 48.0;
    result = pipeline.rebuild(input, 1000.0, 800.0);
    QVERIFY(result.has_value());
    QCOMPARE(result->snapshotGeneration, 3U);
    QCOMPARE(engine.computeCount(), 3);

    input.ephemerisEngine = &otherEngine;
    result = pipeline.rebuild(input, 1000.0, 800.0);
    QVERIFY(result.has_value());
    QCOMPARE(result->snapshotGeneration, 4U);
    QCOMPARE(otherEngine.computeCount(), 1);
}

void SkySceneFramePipelineTests::clearReportsWhetherStateWasPresent()
{
    CountingEngine engine;
    SkySceneFramePipeline pipeline;

    QVERIFY(!pipeline.clear());
    QVERIFY(pipeline.rebuild(makeInput(engine), 1000.0, 800.0).has_value());
    QCOMPARE(pipeline.snapshotGeneration(), 1U);
    QVERIFY(pipeline.clear());
    QCOMPARE(pipeline.snapshotGeneration(), 0U);
    QVERIFY(!pipeline.clear());
}

QTEST_APPLESS_MAIN(SkySceneFramePipelineTests)

#include "SkySceneFramePipelineTests.moc"
