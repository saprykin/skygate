#include <QtTest>

#include "SkySceneModelTestSupport.hpp"

using skygate::ui::tests::makeFixedBody;
using skygate::ui::tests::overlayItemsContainText;
using skygate::ui::tests::SkySceneModelTestHarness;

class SkySceneModelLayerVisibilityTests final : public QObject {
    Q_OBJECT

private slots:
    void themeChangesUpdateRenderedColors();
    void solarSystemLabelsCanBeHidden();
    void constellationLabelsAndLinesCanBeHiddenIndependently();
    void referenceLayerLabelsFollowVisibility();
};

void SkySceneModelLayerVisibilityTests::themeChangesUpdateRenderedColors()
{
    SkySceneModelTestHarness harness({
        makeFixedBody(
            "demo_planet",
            "Demo Planet",
            skygate::ephemeris::CelestialBodyType::Planet,
            -1.0,
            1.5,
            2.5
        ),
    });
    QVERIFY(harness.isValid());
    QVERIFY(harness.centerOnBody("demo_planet"));
    SkyContextController& controller = harness.controller();
    const SkySceneModel& sceneModel = harness.sceneModel();
    const auto demoPlanetState = harness.bodyStateById("demo_planet");
    QVERIFY(demoPlanetState.has_value());

    QColor defaultPointColor;
    for (const auto& point : sceneModel.renderPointSpan()) {
        if (point.bodyIndex == demoPlanetState->bodyIndex) {
            defaultPointColor = point.color;
            break;
        }
    }
    QVERIFY(defaultPointColor.isValid());

    const QVariantList defaultOverlayItems = sceneModel.overlayItems();
    QVERIFY(!defaultOverlayItems.isEmpty());
    const QColor defaultOverlayColor =
        defaultOverlayItems.first().toMap().value("color").value<QColor>();
    QVERIFY(defaultOverlayColor.isValid());

    controller.setThemeId("night-vision");

    QColor nightPointColor;
    for (const auto& point : sceneModel.renderPointSpan()) {
        if (point.bodyIndex == demoPlanetState->bodyIndex) {
            nightPointColor = point.color;
            break;
        }
    }
    QVERIFY(nightPointColor.isValid());
    QVERIFY(defaultPointColor != nightPointColor);

    const QVariantList nightOverlayItems = sceneModel.overlayItems();
    QVERIFY(!nightOverlayItems.isEmpty());
    const QColor nightOverlayColor =
        nightOverlayItems.first().toMap().value("color").value<QColor>();
    QVERIFY(nightOverlayColor.isValid());
    QVERIFY(defaultOverlayColor != nightOverlayColor);
}

void SkySceneModelLayerVisibilityTests::solarSystemLabelsCanBeHidden()
{
    SkySceneModelTestHarness harness({
        makeFixedBody(
            "demo_planet",
            "Demo Planet",
            skygate::ephemeris::CelestialBodyType::Planet,
            -1.0,
            1.5,
            2.5
        ),
    });
    QVERIFY(harness.isValid());
    QVERIFY(harness.centerOnBody("demo_planet"));
    SkyContextController& controller = harness.controller();
    const SkySceneModel& sceneModel = harness.sceneModel();

    QVERIFY(overlayItemsContainText(sceneModel.overlayItems(), "Demo Planet"));

    auto* overlayLayers = qobject_cast<SkyOverlayLayerSettings*>(controller.overlayLayers());
    QVERIFY(overlayLayers != nullptr);
    overlayLayers->setSolarSystemLabels(false);

    QVERIFY(!overlayItemsContainText(sceneModel.overlayItems(), "Demo Planet"));
}

void SkySceneModelLayerVisibilityTests::constellationLabelsAndLinesCanBeHiddenIndependently()
{
    SkySceneModelTestHarness harness({
        makeFixedBody(
            "hip_27989",
            "HIP 27989",
            skygate::ephemeris::CelestialBodyType::Star,
            1.9,
            5.5,
            5.0
        ),
        makeFixedBody(
            "hip_25336",
            "HIP 25336",
            skygate::ephemeris::CelestialBodyType::Star,
            2.1,
            5.5,
            5.0
        ),
        makeFixedBody(
            "hip_25930",
            "HIP 25930",
            skygate::ephemeris::CelestialBodyType::Star,
            2.2,
            5.5,
            5.0
        ),
        makeFixedBody(
            "hip_26311",
            "HIP 26311",
            skygate::ephemeris::CelestialBodyType::Star,
            2.3,
            5.5,
            5.0
        ),
        makeFixedBody(
            "hip_26727",
            "HIP 26727",
            skygate::ephemeris::CelestialBodyType::Star,
            2.4,
            5.5,
            5.0
        ),
        makeFixedBody(
            "hip_24436",
            "HIP 24436",
            skygate::ephemeris::CelestialBodyType::Star,
            2.5,
            5.5,
            5.0
        ),
    });
    QVERIFY(harness.isValid());
    SkyContextController& controller = harness.controller();
    const SkySceneModel& sceneModel = harness.sceneModel();
    QVERIFY(controller.focusSearchTarget("constellationLabel", "Orion"));

    QVERIFY(!sceneModel.renderLineSpan().empty());
    QVERIFY(overlayItemsContainText(sceneModel.overlayItems(), "Orion"));

    auto* overlayLayers = qobject_cast<SkyOverlayLayerSettings*>(controller.overlayLayers());
    QVERIFY(overlayLayers != nullptr);
    overlayLayers->setConstellationLabels(false);

    QVERIFY(!sceneModel.renderLineSpan().empty());
    QVERIFY(!overlayItemsContainText(sceneModel.overlayItems(), "Orion"));

    overlayLayers->setConstellationLines(false);

    QVERIFY(sceneModel.renderLineSpan().empty());
}

void SkySceneModelLayerVisibilityTests::referenceLayerLabelsFollowVisibility()
{
    SkySceneModelTestHarness harness({
        makeFixedBody(
            "demo_star",
            "Demo Star",
            skygate::ephemeris::CelestialBodyType::Star,
            2.0,
            5.5,
            5.0
        ),
    });
    QVERIFY(harness.isValid());
    SkyContextController& controller = harness.controller();
    const SkySceneModel& sceneModel = harness.sceneModel();
    controller.setViewCenter(35.0, 0.0);

    QVERIFY(!overlayItemsContainText(sceneModel.overlayItems(), "Ecliptic"));
    QVERIFY(!overlayItemsContainText(sceneModel.overlayItems(), "Celestial equator"));
    QVERIFY(!overlayItemsContainText(sceneModel.overlayItems(), "Circumpolar"));

    auto* overlayLayers = qobject_cast<SkyOverlayLayerSettings*>(controller.overlayLayers());
    QVERIFY(overlayLayers != nullptr);

    const auto centerOn = [&controller](const skygate::core::HorizontalCoordinate& horizontal) {
        if (!std::isfinite(horizontal.altitudeDeg) || !std::isfinite(horizontal.azimuthDeg)) {
            return false;
        }

        controller.setViewCenter(horizontal.altitudeDeg, horizontal.azimuthDeg);
        return true;
    };

    QVERIFY(centerOn(skygate::ephemeris::CelestialReferenceCalculator::eclipticPoint(
        90.0,
        controller.skyContext().observer,
        controller.skyContext().utcTime
    )));
    overlayLayers->setEcliptic(true);
    QVERIFY(overlayItemsContainText(sceneModel.overlayItems(), "Ecliptic"));
    overlayLayers->setEcliptic(false);
    QVERIFY(!overlayItemsContainText(sceneModel.overlayItems(), "Ecliptic"));

    QVERIFY(centerOn(skygate::ephemeris::CelestialReferenceCalculator::declinationCirclePoint(
        24,
        96,
        0.0,
        controller.skyContext().observer,
        controller.skyContext().utcTime
    )));
    overlayLayers->setCelestialEquator(true);
    QVERIFY(overlayItemsContainText(sceneModel.overlayItems(), "Celestial equator"));
    overlayLayers->setCelestialEquator(false);
    QVERIFY(!overlayItemsContainText(sceneModel.overlayItems(), "Celestial equator"));

    const double boundaryDeclinationDeg =
        skygate::ephemeris::CelestialReferenceCalculator::circumpolarBoundaryDeclinationDeg(
            controller.skyContext().observer
        );
    QVERIFY(centerOn(skygate::ephemeris::CelestialReferenceCalculator::declinationCirclePoint(
        24,
        96,
        boundaryDeclinationDeg,
        controller.skyContext().observer,
        controller.skyContext().utcTime
    )));
    overlayLayers->setCircumpolarBoundary(true);
    QVERIFY(overlayItemsContainText(sceneModel.overlayItems(), "Circumpolar"));
    overlayLayers->setCircumpolarBoundary(false);
    QVERIFY(!overlayItemsContainText(sceneModel.overlayItems(), "Circumpolar"));
}

QTEST_GUILESS_MAIN(SkySceneModelLayerVisibilityTests)

#include "SkySceneModelLayerVisibilityTests.moc"
