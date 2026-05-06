#include "SkyContextControllerTestSupport.hpp"

class SkyContextControllerSearchTrackingTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void focusSearchTargetCentersBodyResult();
    void focusSearchTargetCentersConstellationLabelResult();
    void focusSearchTargetIgnoresInvalidTargets();
    void trackSearchTargetSetsLiveCurrentTimeAndCentersBody();
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
    auto starCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody(
            "demo_target",
            "Demo Target",
            skygate::ephemeris::CelestialBodyType::Star,
            1.0,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 1.5,
                .declinationDeg = 2.5
            }
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
    QVERIFY(
        azimuthDifferenceDeg(
            controller->viewCenterAzimuthDeg(),
            targetState->horizontal.azimuthDeg
        ) < 1e-6
    );
}

void SkyContextControllerSearchTrackingTests::focusSearchTargetCentersConstellationLabelResult()
{
    auto starCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody(
            "hip_27989",
            "HIP 27989",
            skygate::ephemeris::CelestialBodyType::Star,
            2.0,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 5.5,
                .declinationDeg = 5.0
            }
        ),
        makeBody(
            "hip_25336",
            "HIP 25336",
            skygate::ephemeris::CelestialBodyType::Star,
            2.1,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 5.5,
                .declinationDeg = 5.0
            }
        ),
        makeBody(
            "hip_25930",
            "HIP 25930",
            skygate::ephemeris::CelestialBodyType::Star,
            2.2,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 5.5,
                .declinationDeg = 5.0
            }
        ),
        makeBody(
            "hip_26311",
            "HIP 26311",
            skygate::ephemeris::CelestialBodyType::Star,
            2.3,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 5.5,
                .declinationDeg = 5.0
            }
        ),
        makeBody(
            "hip_26727",
            "HIP 26727",
            skygate::ephemeris::CelestialBodyType::Star,
            2.4,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 5.5,
                .declinationDeg = 5.0
            }
        ),
        makeBody(
            "hip_24436",
            "HIP 24436",
            skygate::ephemeris::CelestialBodyType::Star,
            2.5,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 5.5,
                .declinationDeg = 5.0
            }
        ),
    });
    QVERIFY(starCatalog != nullptr);

    auto ephemerisEngine = createTestEphemerisEngine(*starCatalog);
    const auto controller = createController(std::move(starCatalog), std::move(ephemerisEngine));
    configureFocusTestContext(*controller);

    const auto snapshot = controller->ephemerisEngine()->compute(controller->skyContext());
    const auto* targetState = findStateById(snapshot, "hip_27989");
    QVERIFY(targetState != nullptr);
    QVERIFY(
        std::any_of(
            controller->constellationLabelRefs().begin(),
            controller->constellationLabelRefs().end(),
            [](const SkyContextController::ConstellationLabelRef& labelRef) {
                return labelRef.first == "Orion";
            }
        )
    );

    QVERIFY(controller->focusSearchTarget("constellationLabel", "Orion"));
    QCOMPARE(controller->selectedSearchTargetKind(), QString("constellationLabel"));
    QCOMPARE(controller->selectedSearchTargetId(), QString("Orion"));
    QVERIFY(std::abs(controller->viewCenterAltitudeDeg() - targetState->horizontal.altitudeDeg) < 1e-6);
    QVERIFY(
        azimuthDifferenceDeg(
            controller->viewCenterAzimuthDeg(),
            targetState->horizontal.azimuthDeg
        ) < 1e-6
    );
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

void SkyContextControllerSearchTrackingTests::trackSearchTargetSetsLiveCurrentTimeAndCentersBody()
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

    const qint64 timelineSeconds = controllerUtcTime(*controller).toSecsSinceEpoch();
    QCOMPARE(timelineSeconds, fixedNowUtc().toSecsSinceEpoch());

    const auto snapshot = controller->ephemerisEngine()->compute(controller->skyContext());
    const auto* targetState = findStateById(snapshot, "demo_target");
    QVERIFY(targetState != nullptr);
    QVERIFY(
        std::abs(controller->viewCenterAltitudeDeg() - targetState->horizontal.altitudeDeg) < 1e-6
    );
    QVERIFY(
        azimuthDifferenceDeg(
            controller->viewCenterAzimuthDeg(),
            targetState->horizontal.azimuthDeg
        ) < 1e-6
    );
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
    QVERIFY(
        std::abs(controller->viewCenterAltitudeDeg() - targetState->horizontal.altitudeDeg) < 1e-6
    );
    QVERIFY(
        azimuthDifferenceDeg(
            controller->viewCenterAzimuthDeg(),
            targetState->horizontal.azimuthDeg
        ) < 1e-6
    );

    controller->setStepSeconds(3600);
    controller->stepForward();

    snapshot = controller->ephemerisEngine()->compute(controller->skyContext());
    targetState = findStateById(snapshot, "demo_target");
    QVERIFY(targetState != nullptr);
    QVERIFY(
        std::abs(controller->viewCenterAltitudeDeg() - targetState->horizontal.altitudeDeg) < 1e-6
    );
    QVERIFY(
        azimuthDifferenceDeg(
            controller->viewCenterAzimuthDeg(),
            targetState->horizontal.azimuthDeg
        ) < 1e-6
    );
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
    auto starCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody(
            "tracked_target",
            "Tracked Target",
            skygate::ephemeris::CelestialBodyType::Star,
            1.0,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 1.5,
                .declinationDeg = 2.5
            }
        ),
        makeBody(
            "search_target",
            "Search Target",
            skygate::ephemeris::CelestialBodyType::Star,
            1.2,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 8.5,
                .declinationDeg = 12.5
            }
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
    QVERIFY(
        std::abs(controller->viewCenterAltitudeDeg() - searchState->horizontal.altitudeDeg) < 1e-6
    );
    QVERIFY(
        azimuthDifferenceDeg(
            controller->viewCenterAzimuthDeg(),
            searchState->horizontal.azimuthDeg
        ) < 1e-6
    );
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
    auto starCatalog = skygate::ephemeris::createStarCatalogFromBodies({
        makeBody(
            "demo_target",
            "Demo Target",
            skygate::ephemeris::CelestialBodyType::Star,
            1.0,
            skygate::core::EquatorialCoordinate {
                .rightAscensionHours = 1.5,
                .declinationDeg = 2.5
            }
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
