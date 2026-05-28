#include "CelestialBodyCatalog.hpp"
#include "SkySceneFramePipeline.hpp"
#include "engine/EphemerisEngineQueries.hpp"
#include "engine/IEphemerisEngine.hpp"

#include <QtTest/QtTest>

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

class CountingEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    explicit CountingEngine(std::string bodyId = "target") : m_bodyId(std::move(bodyId)) {}

    [[nodiscard]] skygate::ephemeris::EphemerisSnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override
    {
        ++m_requestComputeCount;
        return makeSnapshot(
            request.context,
            request.options.engineKind() == skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision
                ? m_highPrecisionAltitudeDeg
                : m_simpleAltitudeDeg,
            skygate::ephemeris::EphemerisCorrectionFlags::has(
                request.options.correctionFlags(), skygate::ephemeris::EphemerisCorrectionFlags::lightTime()
            )
                ? m_lightTimeAzimuthDeg
                : m_baseAzimuthDeg
        );
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::string_view bodyId) const override
    {
        return computeBodyState(request.context, bodyId);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::size_t bodyIndex) const override
    {
        return computeBodyState(request.context, static_cast<std::uint32_t>(bodyIndex));
    }

    [[nodiscard]] skygate::ephemeris::EphemerisSnapshot
    compute(const skygate::core::ObservationContext& context) const override
    {
        ++m_contextComputeCount;
        return makeSnapshot(context, m_simpleAltitudeDeg, m_baseAzimuthDeg);
    }

    [[nodiscard]] int computeCount() const noexcept
    {
        return m_requestComputeCount + m_contextComputeCount;
    }

    [[nodiscard]] int requestComputeCount() const noexcept
    {
        return m_requestComputeCount;
    }

    [[nodiscard]] int contextComputeCount() const noexcept
    {
        return m_contextComputeCount;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext& context, const std::string_view bodyId) const override
    {
        return skygate::ephemeris::EphemerisEngineQueries::computeBodyStateById(*this, context, bodyId);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext& context, const std::uint32_t bodyIndex) const override
    {
        return skygate::ephemeris::EphemerisEngineQueries::computeBodyStateByIndex(*this, context, bodyIndex);
    }

private:
    [[nodiscard]] skygate::ephemeris::EphemerisSnapshot makeSnapshot(
        const skygate::core::ObservationContext& context, const double altitudeDeg, const double azimuthDeg
    ) const
    {
        skygate::ephemeris::OwnGalaxyCelestialBody body;
        body.id = m_bodyId;
        body.displayName = "Target";
        body.visualMagnitude = 1.0;

        skygate::ephemeris::EphemerisSnapshot snapshot;
        snapshot.context = context;
        std::vector<skygate::ephemeris::OwnGalaxyCelestialBody> bodies;
        bodies.push_back(std::move(body));
        snapshot.catalogBodies = std::make_shared<const skygate::ephemeris::CelestialBodyCatalog>(std::move(bodies));
        snapshot.states.push_back(
            skygate::ephemeris::CelestialBodyState{
                .bodyIndex = 0U, .horizontal = {.altitudeDeg = altitudeDeg, .azimuthDeg = azimuthDeg}
            }
        );
        return snapshot;
    }

    std::string m_bodyId;
    double m_simpleAltitudeDeg = 45.0;
    double m_highPrecisionAltitudeDeg = 55.0;
    double m_baseAzimuthDeg = 180.0;
    double m_lightTimeAzimuthDeg = 181.5;
    mutable int m_requestComputeCount = 0;
    mutable int m_contextComputeCount = 0;
};

SkySceneFramePipelineInput makeInput(const CountingEngine& engine)
{
    SkySceneFramePipelineInput input;
    input.ephemerisEngine = &engine;
    input.skyContext.observer = {.latitudeDeg = 47.0, .longitudeDeg = 8.0, .elevationMeters = 400.0};
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
    void requestBasedSnapshotsUseSelectedEngineOptions();
    void requestOptionChangesOnSameEngineRecomputeSnapshot();
    void highPrecisionRevisionChangesRecomputeSnapshot();
    void astronomicalEpochSubsecondChangesRecomputeSnapshot();
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

void SkySceneFramePipelineTests::requestBasedSnapshotsUseSelectedEngineOptions()
{
    CountingEngine simpleEngine;
    CountingEngine highPrecisionEngine;
    SkySceneFramePipeline pipeline;
    auto input = makeInput(simpleEngine);

    skygate::ephemeris::EphemerisRequest request;
    request.context = input.skyContext;
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::Simple);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::noCorrections());
    input.ephemerisRequest = request;

    auto result = pipeline.rebuild(input, 1000.0, 800.0);
    QVERIFY(result.has_value());
    QCOMPARE(simpleEngine.requestComputeCount(), 1);
    QCOMPARE(simpleEngine.contextComputeCount(), 0);
    QCOMPARE(result->snapshot->states.front().horizontal.altitudeDeg, 45.0);
    QCOMPARE(result->snapshot->states.front().horizontal.azimuthDeg, 180.0);

    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::lightTime());
    input.ephemerisEngine = &highPrecisionEngine;
    input.ephemerisRequest = request;

    result = pipeline.rebuild(input, 1000.0, 800.0);
    QVERIFY(result.has_value());
    QCOMPARE(highPrecisionEngine.requestComputeCount(), 1);
    QCOMPARE(highPrecisionEngine.contextComputeCount(), 0);
    QCOMPARE(result->snapshot->states.front().horizontal.altitudeDeg, 55.0);
    QCOMPARE(result->snapshot->states.front().horizontal.azimuthDeg, 181.5);
    QCOMPARE(result->snapshotGeneration, 2U);
}

void SkySceneFramePipelineTests::requestOptionChangesOnSameEngineRecomputeSnapshot()
{
    CountingEngine engine;
    SkySceneFramePipeline pipeline;
    auto input = makeInput(engine);

    skygate::ephemeris::EphemerisRequest request;
    request.context = input.skyContext;
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::Simple);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::noCorrections());
    input.ephemerisRequest = request;

    const auto first = pipeline.rebuild(input, 1000.0, 800.0);
    QVERIFY(first.has_value());
    QCOMPARE(engine.requestComputeCount(), 1);
    QCOMPARE(engine.contextComputeCount(), 0);
    QCOMPARE(first->snapshotGeneration, 1U);
    QCOMPARE(first->renderFrameGeneration, 1U);
    QVERIFY(!first->frame->points.empty());
    const double firstX = first->frame->points.front().x;
    const double firstY = first->frame->points.front().y;

    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::lightTime());
    input.ephemerisRequest = request;

    const auto second = pipeline.rebuild(input, 1000.0, 800.0);
    QVERIFY(second.has_value());
    QVERIFY(second->updated);
    QCOMPARE(engine.requestComputeCount(), 2);
    QCOMPARE(engine.contextComputeCount(), 0);
    QCOMPARE(second->snapshotGeneration, 2U);
    QCOMPARE(second->renderFrameGeneration, 2U);
    QCOMPARE(second->snapshot->states.front().horizontal.altitudeDeg, 55.0);
    QCOMPARE(second->snapshot->states.front().horizontal.azimuthDeg, 181.5);
    QVERIFY(!second->frame->points.empty());
    QVERIFY(second->frame->points.front().x != firstX || second->frame->points.front().y != firstY);
}

void SkySceneFramePipelineTests::highPrecisionRevisionChangesRecomputeSnapshot()
{
    CountingEngine engine;
    SkySceneFramePipeline pipeline;
    auto input = makeInput(engine);

    skygate::ephemeris::EphemerisRequest request;
    request.context = input.skyContext;
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    input.ephemerisRequest = request;
    input.engineKind = request.options.engineKind();
    input.engineOptionsRevision = 1U;
    input.ephemerisDataRevision = 1U;
    input.earthOrientationDataRevision = 1U;
    input.leapSecondDataRevision = 1U;

    auto result = pipeline.rebuild(input, 1000.0, 800.0);
    QVERIFY(result.has_value());
    QCOMPARE(result->snapshotGeneration, 1U);
    QCOMPARE(engine.requestComputeCount(), 1);

    input.engineOptionsRevision = 2U;
    result = pipeline.rebuild(input, 1000.0, 800.0);
    QVERIFY(result.has_value());
    QCOMPARE(result->snapshotGeneration, 2U);
    QCOMPARE(engine.requestComputeCount(), 2);

    input.ephemerisDataRevision = 2U;
    result = pipeline.rebuild(input, 1000.0, 800.0);
    QVERIFY(result.has_value());
    QCOMPARE(result->snapshotGeneration, 3U);
    QCOMPARE(engine.requestComputeCount(), 3);

    input.earthOrientationDataRevision = 2U;
    result = pipeline.rebuild(input, 1000.0, 800.0);
    QVERIFY(result.has_value());
    QCOMPARE(result->snapshotGeneration, 4U);
    QCOMPARE(engine.requestComputeCount(), 4);

    input.leapSecondDataRevision = 2U;
    result = pipeline.rebuild(input, 1000.0, 800.0);
    QVERIFY(result.has_value());
    QCOMPARE(result->snapshotGeneration, 5U);
    QCOMPARE(engine.requestComputeCount(), 5);

    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::Simple);
    input.ephemerisRequest = request;
    input.engineKind = request.options.engineKind();
    result = pipeline.rebuild(input, 1000.0, 800.0);
    QVERIFY(result.has_value());
    QCOMPARE(result->snapshotGeneration, 6U);
    QCOMPARE(engine.requestComputeCount(), 6);
}

void SkySceneFramePipelineTests::astronomicalEpochSubsecondChangesRecomputeSnapshot()
{
    CountingEngine engine;
    SkySceneFramePipeline pipeline;
    auto input = makeInput(engine);

    skygate::ephemeris::EphemerisRequest request;
    request.context = input.skyContext;
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.epoch = {.julianDatePart1 = 2'451'545.0, .julianDatePart2 = 0.25};
    input.ephemerisRequest = request;
    input.engineKind = request.options.engineKind();

    auto result = pipeline.rebuild(input, 1000.0, 800.0);
    QVERIFY(result.has_value());
    QCOMPARE(result->snapshotGeneration, 1U);
    QCOMPARE(engine.requestComputeCount(), 1);

    request.epoch.julianDatePart2 += 1.0 / 86'400'000.0;
    input.ephemerisRequest = request;
    result = pipeline.rebuild(input, 1000.0, 800.0);
    QVERIFY(result.has_value());
    QCOMPARE(result->snapshotGeneration, 2U);
    QCOMPARE(engine.requestComputeCount(), 2);
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
