#include <QtTest>

#include "SkySceneModelTestSupport.hpp"

#include "EphemerisEngineQueries.hpp"
#include "IEphemerisEngine.hpp"

#include <chrono>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

using skygate::ui::tests::makeFixedBody;
using skygate::ui::tests::SkySceneModelTestHarness;

namespace {

class SnapshotContextEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind kind() const noexcept override
    {
        return skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineOptions options() const noexcept override
    {
        skygate::ephemeris::EphemerisEngineOptions options;
        options.engineKind = kind();
        options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::LightTime;
        return options;
    }

    [[nodiscard]] std::span<const skygate::ephemeris::EphemerisDateRange> supportedDateRanges() const noexcept override
    {
        return m_supportedDateRanges;
    }

    void setSupportedDateRanges(std::vector<skygate::ephemeris::EphemerisDateRange> ranges)
    {
        m_supportedDateRanges = std::move(ranges);
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override
    {
        ++m_requestComputeCount;
        m_lastRequest = request;
        skygate::core::SkyContext resolvedContext = request.context;
        resolvedContext.observer.longitudeDeg += 12.5;
        resolvedContext.utcTime += std::chrono::seconds(75);
        return makeSnapshot(resolvedContext);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::string_view bodyId) const override
    {
        return skygate::ephemeris::EphemerisEngineQueries::findBodyStateById(compute(request), bodyId);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::size_t bodyIndex) const override
    {
        return skygate::ephemeris::EphemerisEngineQueries::findBodyStateByIndex(
            compute(request), static_cast<std::uint32_t>(bodyIndex)
        );
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(const skygate::core::SkyContext& context) const override
    {
        return makeSnapshot(context);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext& context, const std::string_view bodyId) const override
    {
        return skygate::ephemeris::EphemerisEngineQueries::findBodyStateById(compute(context), bodyId);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext& context, const std::uint32_t bodyIndex) const override
    {
        return skygate::ephemeris::EphemerisEngineQueries::findBodyStateByIndex(compute(context), bodyIndex);
    }

    [[nodiscard]] int requestComputeCount() const noexcept
    {
        return m_requestComputeCount;
    }

    [[nodiscard]] const std::optional<skygate::ephemeris::EphemerisRequest>& lastRequest() const noexcept
    {
        return m_lastRequest;
    }

private:
    [[nodiscard]] skygate::ephemeris::SkySnapshot makeSnapshot(const skygate::core::SkyContext& context) const
    {
        auto bodies = std::make_shared<std::vector<skygate::ephemeris::CelestialBody>>();
        bodies->push_back(
            makeFixedBody("resolved", "Resolved", skygate::ephemeris::CelestialBodyType::Star, 1.0, 0.0, 0.0)
        );

        skygate::ephemeris::SkySnapshot snapshot;
        snapshot.context = context;
        snapshot.catalogBodies = bodies;
        snapshot.states.push_back(
            skygate::ephemeris::CelestialBodyState{
                .bodyIndex = 0U,
                .horizontal = {.altitudeDeg = 45.0, .azimuthDeg = 180.0},
            }
        );
        return snapshot;
    }

    mutable int m_requestComputeCount = 0;
    mutable std::optional<skygate::ephemeris::EphemerisRequest> m_lastRequest;
    std::vector<skygate::ephemeris::EphemerisDateRange> m_supportedDateRanges;
};

}  // namespace

class SkySceneModelFrameTests final : public QObject {
    Q_OBJECT

private slots:
    void buildsFrameAndSupportsHitTesting();
    void reusesSnapshotAcrossViewChanges();
    void highPrecisionSceneFrameUsesLeanRenderRequest();
    void degradationReasonsOnlyExposeOutOfRangeKernelSupport();
    void referenceOverlayContextComesFromSelectedRequestSnapshot();
};

void SkySceneModelFrameTests::buildsFrameAndSupportsHitTesting()
{
    SkySceneModelTestHarness harness({
        makeFixedBody("demo_planet", "Demo Planet", skygate::ephemeris::CelestialBodyType::Planet, -1.0, 1.5, 2.5),
    });
    QVERIFY(harness.isValid());
    QVERIFY(harness.centerOnBody("demo_planet"));

    const SkySceneModel& sceneModel = harness.sceneModel();
    QVERIFY(sceneModel.preparedProjection().has_value());
    const auto points = sceneModel.renderPointSpan();
    QVERIFY(!points.empty());

    const SkyRenderPoint* demoPlanetPoint = harness.renderPointForBodyId("demo_planet");
    QVERIFY(demoPlanetPoint != nullptr);

    const QString label = sceneModel.objectLabelAt(demoPlanetPoint->x, demoPlanetPoint->y);
    QCOMPARE(label, QString("Demo Planet"));
    QVERIFY(!sceneModel.overlayItems().isEmpty());
}

void SkySceneModelFrameTests::reusesSnapshotAcrossViewChanges()
{
    SkySceneModelTestHarness harness = SkySceneModelTestHarness::fromBundledCatalog();
    QVERIFY(harness.isValid());
    SkyContextController& controller = harness.controller();
    const SkySceneModel& sceneModel = harness.sceneModel();

    const std::uint64_t initialSnapshotGeneration = sceneModel.snapshotGeneration();
    QVERIFY(initialSnapshotGeneration > 0U);

    controller.setViewCenter(40.0, 210.0);
    QCOMPARE(sceneModel.snapshotGeneration(), initialSnapshotGeneration);

    QVERIFY(controller.setUtcDateTimeText("2024-06-01", "22:30:00"));
    QVERIFY(sceneModel.snapshotGeneration() > initialSnapshotGeneration);
}

void SkySceneModelFrameTests::highPrecisionSceneFrameUsesLeanRenderRequest()
{
    auto starCatalog = skygate::ui::tests::createTestCatalog({
        makeFixedBody("resolved", "Resolved", skygate::ephemeris::CelestialBodyType::Star, 1.0, 0.0, 0.0),
    });
    QVERIFY(starCatalog != nullptr);
    auto engine = std::make_unique<SnapshotContextEngine>();
    const SnapshotContextEngine* enginePtr = engine.get();

    SkyContextController::InitializationOptions options;
    options.loadSettings = false;
    options.initializeLocation = false;
    options.rebuildEphemerisEngineOnStartup = false;
    SkyContextController controller(std::move(starCatalog), std::move(engine), options, nullptr);
    QVERIFY(skygate::ui::tests::configureTestSkyContext(controller));

    SkySceneModel sceneModel;
    sceneModel.setSkyContextController(&controller);
    sceneModel.setViewportSize(1100.0, 760.0);

    QVERIFY(enginePtr->lastRequest().has_value());
    const auto renderCorrections = enginePtr->lastRequest()->options.correctionFlags;
    QVERIFY(!hasCorrectionFlag(renderCorrections, skygate::ephemeris::EphemerisCorrectionFlags::LightTime));
    QVERIFY(hasCorrectionFlag(renderCorrections, skygate::ephemeris::EphemerisCorrectionFlags::PrecessionNutation));
    QVERIFY(hasCorrectionFlag(renderCorrections, skygate::ephemeris::EphemerisCorrectionFlags::EarthOrientation));
    QVERIFY(hasCorrectionFlag(renderCorrections, skygate::ephemeris::EphemerisCorrectionFlags::DiurnalParallax));
}

void SkySceneModelFrameTests::degradationReasonsOnlyExposeOutOfRangeKernelSupport()
{
    auto starCatalog = skygate::ui::tests::createTestCatalog({
        makeFixedBody("resolved", "Resolved", skygate::ephemeris::CelestialBodyType::Star, 1.0, 0.0, 0.0),
    });
    QVERIFY(starCatalog != nullptr);

    auto engine = std::make_unique<SnapshotContextEngine>();
    skygate::ephemeris::EphemerisDateRange range;
    range.id = "de440s-range";
    range.displayName = "DE440s kernel range";
    range.start = *skygate::ephemeris::astronomicalEpochFromCivilDateTime(
        skygate::ephemeris::CivilDateTime{.astronomicalYear = 1849, .month = 12, .day = 26}
    );
    range.end = *skygate::ephemeris::astronomicalEpochFromCivilDateTime(
        skygate::ephemeris::CivilDateTime{.astronomicalYear = 2150, .month = 1, .day = 22}
    );
    engine->setSupportedDateRanges({range});

    SkyContextController::InitializationOptions options;
    options.loadSettings = false;
    options.initializeLocation = false;
    options.rebuildEphemerisEngineOnStartup = false;
    SkyContextController controller(std::move(starCatalog), std::move(engine), options, nullptr);
    QVERIFY(skygate::ui::tests::configureTestSkyContext(controller));
    QVERIFY(controller.setUtcDateTimeText(QStringLiteral("10000-01-01 BCE"), QStringLiteral("00:00:00")));

    SkySceneModel sceneModel;
    sceneModel.setSkyContextController(&controller);
    sceneModel.setViewportSize(1100.0, 760.0);

    const QVariantList reasons = sceneModel.ephemerisDegradationReasons();
    QCOMPARE(reasons.size(), 1);
    const QString reason = reasons.front().toString();
    QVERIFY(reason.contains(QStringLiteral("Planetary kernel out of range")));
    QVERIFY(reason.contains(QStringLiteral("DE440s kernel range: 1849-12-26 to 2150-01-22")));
}

void SkySceneModelFrameTests::referenceOverlayContextComesFromSelectedRequestSnapshot()
{
    auto starCatalog = skygate::ui::tests::createTestCatalog({
        makeFixedBody("resolved", "Resolved", skygate::ephemeris::CelestialBodyType::Star, 1.0, 0.0, 0.0),
    });
    QVERIFY(starCatalog != nullptr);
    auto engine = std::make_unique<SnapshotContextEngine>();
    const SnapshotContextEngine* enginePtr = engine.get();

    SkyContextController::InitializationOptions options;
    options.loadSettings = false;
    options.initializeLocation = false;
    options.rebuildEphemerisEngineOnStartup = false;
    SkyContextController controller(std::move(starCatalog), std::move(engine), options, nullptr);
    QVERIFY(skygate::ui::tests::configureTestSkyContext(controller));

    SkySceneModel sceneModel;
    sceneModel.setSkyContextController(&controller);
    sceneModel.setViewportSize(1100.0, 760.0);

    const auto overlayContext = sceneModel.referenceOverlayContext();
    QVERIFY(overlayContext.has_value());
    QVERIFY(enginePtr->requestComputeCount() > 0);
    QCOMPARE(overlayContext->observer.longitudeDeg, controller.skyContext().observer.longitudeDeg + 12.5);
    QCOMPARE(overlayContext->utcTime, controller.skyContext().utcTime + std::chrono::seconds(75));
}

QTEST_GUILESS_MAIN(SkySceneModelFrameTests)

#include "SkySceneModelFrameTests.moc"
