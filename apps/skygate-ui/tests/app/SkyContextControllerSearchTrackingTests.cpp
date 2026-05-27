#include "ConstellationTestSupport.hpp"
#include "SkyContextControllerTestSupport.hpp"
#include "SkyObjectSearchModel.hpp"

#include <memory>
#include <string_view>
#include <vector>

namespace {

class RequestSensitiveEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    explicit RequestSensitiveEngine(std::vector<skygate::ephemeris::CelestialBody> bodies)
        : m_bodies(std::make_shared<const std::vector<skygate::ephemeris::CelestialBody>>(std::move(bodies)))
    {
        m_options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
        m_options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::lightTime());
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind::Type kind() const noexcept override
    {
        return skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    }

    [[nodiscard]] std::string_view name() const noexcept override
    {
        return "Request-sensitive test engine";
    }

    [[nodiscard]] skygate::ephemeris::EphemerisCapabilities capabilities() const noexcept override
    {
        skygate::ephemeris::EphemerisCapabilities capabilities =
            skygate::ephemeris::EphemerisCapabilities::noCapabilities();

        capabilities = capabilities | skygate::ephemeris::EphemerisCapabilities::catalogStars();
        capabilities = capabilities | skygate::ephemeris::EphemerisCapabilities::topocentricPositions();
        return capabilities;
    }

    [[nodiscard]] std::span<const skygate::ephemeris::EphemerisDateRange> supportedDateRanges() const noexcept override
    {
        return {};
    }

    [[nodiscard]] skygate::ephemeris::EphemerisDataSetInfo dataSetInfo() const override
    {
        skygate::ephemeris::EphemerisDataSetInfo info;
        info.id = "request-sensitive-test";
        return info;
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineOptions options() const noexcept override
    {
        return m_options;
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override
    {
        ++m_requestComputeCount;
        const bool highPrecisionRequest =
            request.options.engineKind() == skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
        const bool lightTimeRequest = skygate::ephemeris::EphemerisCorrectionFlags::has(
            request.options.correctionFlags(), skygate::ephemeris::EphemerisCorrectionFlags::lightTime()
        );
        return makeSnapshot(request.context, highPrecisionRequest && lightTimeRequest ? 64.0 : 41.0, 222.0);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::string_view bodyId) const override
    {
        return findState(compute(request), bodyId);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::size_t bodyIndex) const override
    {
        const auto snapshot = compute(request);
        if (bodyIndex >= snapshot.states.size()) {
            return std::nullopt;
        }
        return snapshot.states[bodyIndex];
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(const skygate::core::SkyContext& context) const override
    {
        ++m_contextComputeCount;
        return makeSnapshot(context, 12.0, 34.0);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext& context, const std::string_view bodyId) const override
    {
        return findState(compute(context), bodyId);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext& context, const std::uint32_t bodyIndex) const override
    {
        const auto snapshot = compute(context);
        if (bodyIndex >= snapshot.states.size()) {
            return std::nullopt;
        }
        return snapshot.states[bodyIndex];
    }

    [[nodiscard]] int requestComputeCount() const noexcept
    {
        return m_requestComputeCount;
    }

    [[nodiscard]] int contextComputeCount() const noexcept
    {
        return m_contextComputeCount;
    }

private:
    [[nodiscard]] skygate::ephemeris::SkySnapshot
    makeSnapshot(const skygate::core::SkyContext& context, const double altitudeDeg, const double azimuthDeg) const
    {
        skygate::ephemeris::SkySnapshot snapshot;
        snapshot.context = context;
        snapshot.catalogBodies = m_bodies;
        for (std::size_t bodyIndex = 0; bodyIndex < m_bodies->size(); ++bodyIndex) {
            snapshot.states.push_back(
                skygate::ephemeris::CelestialBodyState{
                    .bodyIndex = static_cast<std::uint32_t>(bodyIndex),
                    .horizontal = {.altitudeDeg = altitudeDeg, .azimuthDeg = azimuthDeg}
                }
            );
        }
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    findState(const skygate::ephemeris::SkySnapshot& snapshot, const std::string_view bodyId) const
    {
        for (const auto& state : snapshot.states) {
            if (snapshot.bodyAt(state.bodyIndex).id == bodyId) {
                return state;
            }
        }
        return std::nullopt;
    }

    std::shared_ptr<const std::vector<skygate::ephemeris::CelestialBody>> m_bodies;
    skygate::ephemeris::EphemerisEngineOptions m_options;
    mutable int m_requestComputeCount = 0;
    mutable int m_contextComputeCount = 0;
};

class BodyLookupCountingEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    explicit BodyLookupCountingEngine(std::vector<skygate::ephemeris::CelestialBody> bodies)
        : m_bodies(std::make_shared<const std::vector<skygate::ephemeris::CelestialBody>>(std::move(bodies)))
    {
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind::Type kind() const noexcept override
    {
        return skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineOptions options() const noexcept override
    {
        skygate::ephemeris::EphemerisEngineOptions options;
        options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
        options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::lightTime());
        return options;
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override
    {
        ++m_requestComputeCount;
        return makeSnapshot(request.context);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::string_view bodyId) const override
    {
        for (std::size_t bodyIndex = 0; bodyIndex < m_bodies->size(); ++bodyIndex) {
            if ((*m_bodies)[bodyIndex].id == bodyId) {
                return computeBodyState(request, bodyIndex);
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest&, const std::size_t bodyIndex) const override
    {
        if (bodyIndex >= m_bodies->size()) {
            return std::nullopt;
        }

        ++m_requestBodyStateCount;
        return skygate::ephemeris::CelestialBodyState{
            .bodyIndex = static_cast<std::uint32_t>(bodyIndex),
            .horizontal = {.altitudeDeg = 23.0, .azimuthDeg = 42.0},
        };
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(const skygate::core::SkyContext& context) const override
    {
        ++m_contextComputeCount;
        return makeSnapshot(context);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext&, const std::string_view bodyId) const override
    {
        for (std::size_t bodyIndex = 0; bodyIndex < m_bodies->size(); ++bodyIndex) {
            if ((*m_bodies)[bodyIndex].id == bodyId) {
                return computeBodyState(skygate::ephemeris::EphemerisRequest{}, bodyIndex);
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext&, const std::uint32_t bodyIndex) const override
    {
        return computeBodyState(skygate::ephemeris::EphemerisRequest{}, static_cast<std::size_t>(bodyIndex));
    }

    [[nodiscard]] int requestComputeCount() const noexcept
    {
        return m_requestComputeCount;
    }

    [[nodiscard]] int contextComputeCount() const noexcept
    {
        return m_contextComputeCount;
    }

    [[nodiscard]] int requestBodyStateCount() const noexcept
    {
        return m_requestBodyStateCount;
    }

private:
    [[nodiscard]] skygate::ephemeris::SkySnapshot makeSnapshot(const skygate::core::SkyContext& context) const
    {
        skygate::ephemeris::SkySnapshot snapshot;
        snapshot.context = context;
        snapshot.catalogBodies = m_bodies;
        snapshot.states.reserve(m_bodies->size());
        for (std::size_t bodyIndex = 0; bodyIndex < m_bodies->size(); ++bodyIndex) {
            snapshot.states.push_back(
                skygate::ephemeris::CelestialBodyState{
                    .bodyIndex = static_cast<std::uint32_t>(bodyIndex),
                    .horizontal = {.altitudeDeg = 23.0, .azimuthDeg = 42.0},
                }
            );
        }
        return snapshot;
    }

    std::shared_ptr<const std::vector<skygate::ephemeris::CelestialBody>> m_bodies;
    mutable int m_requestComputeCount = 0;
    mutable int m_contextComputeCount = 0;
    mutable int m_requestBodyStateCount = 0;
};

std::unique_ptr<SkyContextController> createRequestSensitiveController(
    RequestSensitiveEngine*& engine, const skygate::core::ITimeSource* timeSource = nullptr
)
{
    std::vector<skygate::ephemeris::CelestialBody> bodies{makeBody(
        "demo_target",
        "Demo Target",
        skygate::ephemeris::CelestialBodyType::Star,
        1.0,
        skygate::core::EquatorialCoordinate{.rightAscensionHours = 1.5, .declinationDeg = 2.5}
    )};
    auto starCatalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(bodies);
    Q_ASSERT(starCatalog != nullptr);

    auto ephemerisEngine = std::make_unique<RequestSensitiveEngine>(std::move(bodies));
    engine = ephemerisEngine.get();

    auto initializationOptions = controllerInitializationOptions(false, timeSource);
    initializationOptions.rebuildEphemerisEngineOnStartup = false;
    auto controller = std::make_unique<SkyContextController>(
        std::move(starCatalog), std::move(ephemerisEngine), initializationOptions, nullptr
    );
    configureFocusTestContext(*controller);
    return controller;
}

QStringList searchTargetIds(const SkyObjectSearchModel& model)
{
    QStringList targetIds;
    for (int row = 0; row < model.rowCount(); ++row) {
        targetIds.push_back(model.index(row, 0).data(SkyObjectSearchModel::TargetIdRole).toString());
    }
    return targetIds;
}

}  // namespace

class SkyContextControllerSearchTrackingTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void focusSearchTargetCentersBodyResult();
    void focusSearchTargetUsesSelectedEngineRequest();
    void focusSearchTargetUsesSingleBodyLookupForMoon();
    void focusSearchTargetCentersConstellationLabelResult();
    void focusSearchTargetIgnoresInvalidTargets();
    void trackSearchTargetStartsLiveAtTimelineTimeAndCentersBody();
    void trackSearchTargetPreservesBceTimelineAnchor();
    void trackedBceTargetPlaybackUsesConfiguredStepWhileCatchingUp();
    void trackSearchTargetUsesSelectedEngineRequest();
    void searchModelRemainsCatalogAndLabelOnly();
    void trackSearchTargetRejectsInvalidTargetsWithoutMutation();
    void trackedTargetRecentersOnStepAndManualPan();
    void staleTrackedTargetClearsAndAllowsViewCenterChanges();
    void focusSearchTargetClearsTrackingForDifferentTarget();
    void clearingTrackedTargetPreservesSelectedSearchTarget();
    void collapsingSearchToolbarClearsSelectedSearchTarget();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyContextControllerSearchTrackingTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyContextControllerSearchTrackingTests")));
}

void SkyContextControllerSearchTrackingTests::init()
{
    m_settings.resetForCurrentTest();
}

void SkyContextControllerSearchTrackingTests::focusSearchTargetCentersBodyResult()
{
    auto starCatalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({
        makeBody(
            "demo_target",
            "Demo Target",
            skygate::ephemeris::CelestialBodyType::Star,
            1.0,
            skygate::core::EquatorialCoordinate{.rightAscensionHours = 1.5, .declinationDeg = 2.5}
        ),
    });
    QVERIFY(starCatalog != nullptr);

    auto ephemerisEngine = createTestEphemerisEngine(*starCatalog);
    const auto controller = createController(std::move(starCatalog), std::move(ephemerisEngine));
    configureFocusTestContext(*controller);

    const auto snapshot = controller->ephemerisEngine()->compute(controller->skyContext());
    const auto* targetState = findStateById(snapshot, "demo_target");
    QVERIFY(targetState != nullptr);

    QVERIFY(controller->focusSearchTarget("body", "demo_target"));
    QCOMPARE(controller->selectedSearchTargetKind(), QString("body"));
    QCOMPARE(controller->selectedSearchTargetId(), QString("demo_target"));
    QVERIFY(std::abs(controller->viewCenterAltitudeDeg() - targetState->horizontal.altitudeDeg) < 1e-6);
    QVERIFY(azimuthDifferenceDeg(controller->viewCenterAzimuthDeg(), targetState->horizontal.azimuthDeg) < 1e-6);
}

void SkyContextControllerSearchTrackingTests::focusSearchTargetUsesSelectedEngineRequest()
{
    RequestSensitiveEngine* engine = nullptr;
    const auto controller = createRequestSensitiveController(engine);
    QVERIFY(engine != nullptr);

    QVERIFY(controller->focusSearchTarget("body", "demo_target"));

    QVERIFY(engine->requestComputeCount() > 0);
    QCOMPARE(engine->contextComputeCount(), 0);
    QCOMPARE(controller->selectedSearchTargetKind(), QString("body"));
    QCOMPARE(controller->selectedSearchTargetId(), QString("demo_target"));
    QCOMPARE(controller->viewCenterAltitudeDeg(), 64.0);
    QCOMPARE(controller->viewCenterAzimuthDeg(), 222.0);
}

void SkyContextControllerSearchTrackingTests::focusSearchTargetUsesSingleBodyLookupForMoon()
{
    std::vector<skygate::ephemeris::CelestialBody> bodies{
        makeBody("moon", "Moon", skygate::ephemeris::CelestialBodyType::Moon, -12.0)
    };
    bodies.front().ephemerisSource = skygate::ephemeris::CelestialBodyEphemerisSource::Moon;
    auto starCatalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(bodies);
    QVERIFY(starCatalog != nullptr);

    auto ephemerisEngine = std::make_unique<BodyLookupCountingEngine>(std::move(bodies));
    const auto* engine = ephemerisEngine.get();
    auto initializationOptions = controllerInitializationOptions(false);
    initializationOptions.rebuildEphemerisEngineOnStartup = false;
    auto controller = std::make_unique<SkyContextController>(
        std::move(starCatalog), std::move(ephemerisEngine), initializationOptions, nullptr
    );
    configureFocusTestContext(*controller);

    QVERIFY(controller->focusSearchTarget("body", "moon"));

    QCOMPARE(engine->requestComputeCount(), 0);
    QCOMPARE(engine->contextComputeCount(), 0);
    QCOMPARE(engine->requestBodyStateCount(), 1);
    QCOMPARE(controller->selectedSearchTargetKind(), QString("body"));
    QCOMPARE(controller->selectedSearchTargetId(), QString("moon"));
    QCOMPARE(controller->viewCenterAltitudeDeg(), 23.0);
    QCOMPARE(controller->viewCenterAzimuthDeg(), 42.0);
}

void SkyContextControllerSearchTrackingTests::focusSearchTargetCentersConstellationLabelResult()
{
    QVERIFY(skygate::ui::tests::seedOrionConstellationCache());
    const auto controller = createController();
    skygate::ui::tests::restoreSeededCatalogCache(*controller);
    configureFocusTestContext(*controller);

    const auto snapshot = controller->ephemerisEngine()->compute(controller->skyContext());
    const auto* targetState = findStateById(snapshot, "hip_27989");
    QVERIFY(targetState != nullptr);
    QVERIFY(
        std::any_of(
            controller->constellationAnchorGroups().begin(),
            controller->constellationAnchorGroups().end(),
            [](const SkyContextController::ConstellationAnchorGroup& anchorGroup) {
                return anchorGroup.first == "Orion";
            }
        )
    );

    QVERIFY(controller->focusSearchTarget("constellationLabel", "Orion"));
    QCOMPARE(controller->selectedSearchTargetKind(), QString("constellationLabel"));
    QCOMPARE(controller->selectedSearchTargetId(), QString("Orion"));
    QVERIFY(std::abs(controller->viewCenterAltitudeDeg() - targetState->horizontal.altitudeDeg) < 1e-6);
    QVERIFY(azimuthDifferenceDeg(controller->viewCenterAzimuthDeg(), targetState->horizontal.azimuthDeg) < 1e-6);
}

void SkyContextControllerSearchTrackingTests::focusSearchTargetIgnoresInvalidTargets()
{
    const auto controller = createController();
    controller->setViewCenter(12.0, 123.0);

    QVERIFY(!controller->focusSearchTarget("body", "missing_target"));
    QCOMPARE(controller->viewCenterAltitudeDeg(), 12.0);
    QCOMPARE(controller->viewCenterAzimuthDeg(), 123.0);

    QVERIFY(!controller->focusSearchTarget("unknown", "mars"));
    QCOMPARE(controller->viewCenterAltitudeDeg(), 12.0);
    QCOMPARE(controller->viewCenterAzimuthDeg(), 123.0);
}

void SkyContextControllerSearchTrackingTests::trackSearchTargetStartsLiveAtTimelineTimeAndCentersBody()
{
    FakeTimeSource timeSource;
    const auto controller = createSingleBodyController("demo_target", "Demo Target", &timeSource);
    controller->setLive(false);
    QVERIFY(controller->setUtcDateTimeText("2000-01-01", "00:00:00"));
    controller->setViewCenter(12.0, 123.0);

    QVERIFY(controller->trackSearchTarget("body", "demo_target"));

    QVERIFY(controller->live());
    QVERIFY(controller->hasTrackedTarget());
    QCOMPARE(controller->trackedTargetKind(), QString("body"));
    QCOMPARE(controller->trackedTargetId(), QString("demo_target"));
    QCOMPARE(controller->trackedTargetDisplayText(), QString("Demo Target"));
    QCOMPARE(controller->selectedSearchTargetKind(), QString("body"));
    QCOMPARE(controller->selectedSearchTargetId(), QString("demo_target"));

    const QDateTime timelineUtc = controllerUtcTime(*controller);
    QCOMPARE(timelineUtc, QDateTime(QDate(2000, 1, 1), QTime(0, 0, 0), QTimeZone::UTC));

    const auto snapshot = controller->ephemerisEngine()->compute(controller->skyContext());
    const auto* targetState = findStateById(snapshot, "demo_target");
    QVERIFY(targetState != nullptr);
    QVERIFY(std::abs(controller->viewCenterAltitudeDeg() - targetState->horizontal.altitudeDeg) < 1e-6);
    QVERIFY(azimuthDifferenceDeg(controller->viewCenterAzimuthDeg(), targetState->horizontal.azimuthDeg) < 1e-6);
}

void SkyContextControllerSearchTrackingTests::trackSearchTargetPreservesBceTimelineAnchor()
{
    FakeTimeSource timeSource;
    const auto controller = createSingleBodyController("demo_target", "Demo Target", &timeSource);
    controller->setLive(false);
    QVERIFY(controller->setUtcDateTimeText("0044-03-15 BCE", "12:00:00"));

    QVERIFY(controller->trackSearchTarget("body", "demo_target"));

    QVERIFY(controller->live());
    QVERIFY(controller->hasTrackedTarget());
    QCOMPARE(controller->utcDateText(), QString("0044-03-15 BCE"));
    QCOMPARE(controller->utcTimeText(), QString("12:00:00"));
    QCOMPARE(controllerUtcTime(*controller).date(), QDate(-44, 3, 15));
    QCOMPARE(controllerUtcTime(*controller).time(), QTime(12, 0, 0));
}

void SkyContextControllerSearchTrackingTests::trackedBceTargetPlaybackUsesConfiguredStepWhileCatchingUp()
{
    FakeTimeSource timeSource;
    const auto controller = createSingleBodyController("demo_target", "Demo Target", &timeSource);
    controller->setLive(false);
    controller->setStepSeconds(3600);
    QVERIFY(controller->setUtcDateTimeText("0044-03-15 BCE", "12:00:00"));

    QSignalSpy skyContextChangedSpy(controller.get(), &SkyContextController::skyContextChanged);
    skyContextChangedSpy.clear();

    const qint64 beforeSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    QVERIFY(controller->trackSearchTarget("body", "demo_target"));

    skyContextChangedSpy.clear();
    QTRY_VERIFY_WITH_TIMEOUT(skyContextChangedSpy.count() >= 1, 1500);

    const qint64 afterSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    QVERIFY(afterSeconds - beforeSeconds >= 3600);
    QVERIFY(afterSeconds <= fixedNowUtc().toSecsSinceEpoch());

    controller->setLive(false);
}

void SkyContextControllerSearchTrackingTests::trackSearchTargetUsesSelectedEngineRequest()
{
    FakeTimeSource timeSource;
    RequestSensitiveEngine* engine = nullptr;
    const auto controller = createRequestSensitiveController(engine, &timeSource);
    QVERIFY(engine != nullptr);
    controller->setLive(false);
    QVERIFY(controller->setUtcDateTimeText("2000-01-01", "00:00:00"));

    QVERIFY(controller->trackSearchTarget("body", "demo_target"));

    QVERIFY(engine->requestComputeCount() > 0);
    QCOMPARE(engine->contextComputeCount(), 0);
    QCOMPARE(controller->trackedTargetKind(), QString("body"));
    QCOMPARE(controller->trackedTargetId(), QString("demo_target"));
    QCOMPARE(controller->trackedTargetDisplayText(), QString("Demo Target"));
    QCOMPARE(controller->viewCenterAltitudeDeg(), 64.0);
    QCOMPARE(controller->viewCenterAzimuthDeg(), 222.0);
    QCOMPARE(controllerUtcTime(*controller), QDateTime(QDate(2000, 1, 1), QTime(0, 0, 0), QTimeZone::UTC));
}

void SkyContextControllerSearchTrackingTests::searchModelRemainsCatalogAndLabelOnly()
{
    RequestSensitiveEngine* engine = nullptr;
    const auto controller = createRequestSensitiveController(engine);
    QVERIFY(engine != nullptr);
    auto* searchModel = qobject_cast<SkyObjectSearchModel*>(controller->objectSearchModel());
    QVERIFY(searchModel != nullptr);

    searchModel->setFilterText(QStringLiteral("demo"));
    const QStringList targetIdsBeforeFocus = searchTargetIds(*searchModel);

    QVERIFY(controller->focusSearchTarget("body", "demo_target"));
    searchModel->setFilterText(QStringLiteral("demo"));

    QCOMPARE(searchTargetIds(*searchModel), targetIdsBeforeFocus);
    QVERIFY(targetIdsBeforeFocus.contains(QStringLiteral("demo_target")));
}

void SkyContextControllerSearchTrackingTests::trackSearchTargetRejectsInvalidTargetsWithoutMutation()
{
    const auto controller = createSingleBodyController();
    controller->setLive(false);
    QVERIFY(controller->setUtcDateTimeText("2024-06-01", "22:00:00"));
    controller->setViewCenter(12.0, 123.0);

    const QDateTime initialUtc = controllerUtcTime(*controller);
    QVERIFY(!controller->trackSearchTarget("body", "missing_target"));
    QVERIFY(!controller->hasTrackedTarget());
    QVERIFY(!controller->live());
    QVERIFY(controller->selectedSearchTargetKind().isEmpty());
    QVERIFY(controller->selectedSearchTargetId().isEmpty());
    QCOMPARE(controllerUtcTime(*controller), initialUtc);
    QCOMPARE(controller->viewCenterAltitudeDeg(), 12.0);
    QCOMPARE(controller->viewCenterAzimuthDeg(), 123.0);

    QVERIFY(!controller->trackSearchTarget("constellationLabel", "Orion"));
    QVERIFY(!controller->hasTrackedTarget());
    QCOMPARE(controllerUtcTime(*controller), initialUtc);
}

void SkyContextControllerSearchTrackingTests::trackedTargetRecentersOnStepAndManualPan()
{
    const auto controller = createSingleBodyController();

    QVERIFY(controller->trackSearchTarget("body", "demo_target"));
    controller->panViewBy(15.0, -10.0);

    auto snapshot = controller->ephemerisEngine()->compute(controller->skyContext());
    const auto* targetState = findStateById(snapshot, "demo_target");
    QVERIFY(targetState != nullptr);
    QVERIFY(std::abs(controller->viewCenterAltitudeDeg() - targetState->horizontal.altitudeDeg) < 1e-6);
    QVERIFY(azimuthDifferenceDeg(controller->viewCenterAzimuthDeg(), targetState->horizontal.azimuthDeg) < 1e-6);

    controller->setStepSeconds(3600);
    controller->stepForward();

    snapshot = controller->ephemerisEngine()->compute(controller->skyContext());
    targetState = findStateById(snapshot, "demo_target");
    QVERIFY(targetState != nullptr);
    QVERIFY(std::abs(controller->viewCenterAltitudeDeg() - targetState->horizontal.altitudeDeg) < 1e-6);
    QVERIFY(azimuthDifferenceDeg(controller->viewCenterAzimuthDeg(), targetState->horizontal.azimuthDeg) < 1e-6);
}

void SkyContextControllerSearchTrackingTests::staleTrackedTargetClearsAndAllowsViewCenterChanges()
{
    const auto controller = createSingleBodyController();

    QVERIFY(controller->trackSearchTarget("body", "demo_target"));
    QVERIFY(controller->hasTrackedTarget());

    controller->loadCatalogPreset("bundled");

    QVERIFY(!controller->hasTrackedTarget());
    controller->setViewCenter(12.0, 123.0);
    QCOMPARE(controller->viewCenterAltitudeDeg(), 12.0);
    QCOMPARE(controller->viewCenterAzimuthDeg(), 123.0);
}

void SkyContextControllerSearchTrackingTests::focusSearchTargetClearsTrackingForDifferentTarget()
{
    auto starCatalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({
        makeBody(
            "tracked_target",
            "Tracked Target",
            skygate::ephemeris::CelestialBodyType::Star,
            1.0,
            skygate::core::EquatorialCoordinate{.rightAscensionHours = 1.5, .declinationDeg = 2.5}
        ),
        makeBody(
            "search_target",
            "Search Target",
            skygate::ephemeris::CelestialBodyType::Star,
            1.2,
            skygate::core::EquatorialCoordinate{.rightAscensionHours = 8.5, .declinationDeg = 12.5}
        ),
    });
    QVERIFY(starCatalog != nullptr);

    auto ephemerisEngine = createTestEphemerisEngine(*starCatalog);
    const auto controller = createController(std::move(starCatalog), std::move(ephemerisEngine));
    configureFocusTestContext(*controller);

    QVERIFY(controller->trackSearchTarget("body", "tracked_target"));
    QVERIFY(controller->hasTrackedTarget());

    const auto snapshot = controller->ephemerisEngine()->compute(controller->skyContext());
    const auto* searchState = findStateById(snapshot, "search_target");
    QVERIFY(searchState != nullptr);

    QVERIFY(controller->focusSearchTarget("body", "search_target"));
    QVERIFY(!controller->hasTrackedTarget());
    QCOMPARE(controller->selectedSearchTargetKind(), QString("body"));
    QCOMPARE(controller->selectedSearchTargetId(), QString("search_target"));
    QVERIFY(std::abs(controller->viewCenterAltitudeDeg() - searchState->horizontal.altitudeDeg) < 1e-6);
    QVERIFY(azimuthDifferenceDeg(controller->viewCenterAzimuthDeg(), searchState->horizontal.azimuthDeg) < 1e-6);
}

void SkyContextControllerSearchTrackingTests::clearingTrackedTargetPreservesSelectedSearchTarget()
{
    const auto controller = createSingleBodyController();

    QVERIFY(controller->trackSearchTarget("body", "demo_target"));
    controller->clearTrackedTarget();

    QVERIFY(!controller->hasTrackedTarget());
    QVERIFY(controller->trackedTargetKind().isEmpty());
    QVERIFY(controller->trackedTargetId().isEmpty());
    QVERIFY(controller->trackedTargetDisplayText().isEmpty());
    QCOMPARE(controller->selectedSearchTargetKind(), QString("body"));
    QCOMPARE(controller->selectedSearchTargetId(), QString("demo_target"));
}

void SkyContextControllerSearchTrackingTests::collapsingSearchToolbarClearsSelectedSearchTarget()
{
    auto starCatalog = skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies({
        makeBody(
            "demo_target",
            "Demo Target",
            skygate::ephemeris::CelestialBodyType::Star,
            1.0,
            skygate::core::EquatorialCoordinate{.rightAscensionHours = 1.5, .declinationDeg = 2.5}
        ),
    });
    QVERIFY(starCatalog != nullptr);

    auto ephemerisEngine = createTestEphemerisEngine(*starCatalog);
    const auto controller = createController(std::move(starCatalog), std::move(ephemerisEngine));
    configureFocusTestContext(*controller);

    QVERIFY(controller->focusSearchTarget("body", "demo_target"));
    QCOMPARE(controller->selectedSearchTargetKind(), QString("body"));
    QCOMPARE(controller->selectedSearchTargetId(), QString("demo_target"));

    controller->setSearchToolbarCollapsed(true);
    QVERIFY(controller->selectedSearchTargetKind().isEmpty());
    QVERIFY(controller->selectedSearchTargetId().isEmpty());
}

QTEST_GUILESS_MAIN(SkyContextControllerSearchTrackingTests)

#include "SkyContextControllerSearchTrackingTests.moc"
