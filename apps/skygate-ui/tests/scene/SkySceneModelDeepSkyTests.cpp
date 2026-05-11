#include <QtTest>

#include "SkySceneModelTestSupport.hpp"

using skygate::ui::tests::buildDeepSkyRenderFrame;
using skygate::ui::tests::createTestCatalog;
using skygate::ui::tests::inspectorFieldValue;
using skygate::ui::tests::makeBody;
using skygate::ui::tests::makeDeepSkyBody;
using skygate::ui::tests::overlayItemsContainKind;
using skygate::ui::tests::overlayItemsContainText;
using skygate::ui::tests::SkySceneModelTestHarness;
using skygate::ui::tests::TestSkyContextConfig;

class SkySceneModelDeepSkyTests final : public QObject {
    Q_OBJECT

private slots:
    void deepSkyInspectorIncludesAliasesSizeAndSource();
    void primaryDeepSkyObjectKeepsSourceWhenMergingDeepSkyCatalog();
    void deepSkyObjectsRenderAndCanBeHidden();
    void denseDeepSkyLabelsAreBudgeted();
    void wideDeepSkyLabelsPreferNamedObjects();
    void deepestDeepSkyZoomShowsSeparatedAnonymousLabels();
};

void SkySceneModelDeepSkyTests::deepSkyInspectorIncludesAliasesSizeAndSource()
{
    skygate::ephemeris::CelestialBody m31 = makeDeepSkyBody(
        "messier_031",
        "M31",
        3.44,
        {"M 31", "NGC 224", "Andromeda Galaxy"}
    );
    m31.fixedEquatorial = skygate::core::EquatorialCoordinate {
        .rightAscensionHours = 0.7123,
        .declinationDeg = 41.269
    };

    TestSkyContextConfig contextConfig;
    contextConfig.utcDate = QStringLiteral("2024-09-01");
    SkySceneModelTestHarness harness({std::move(m31)}, contextConfig);
    QVERIFY(harness.isValid());
    QVERIFY(harness.centerOnBody("messier_031"));
    SkyContextController& controller = harness.controller();
    SkySceneModel& sceneModel = harness.sceneModel();

    QVERIFY(!sceneModel.renderGlyphSpan().empty());
    const std::optional<SkyRenderGlyph> glyph = harness.renderGlyphWithLabel(QStringLiteral("M31"));
    QVERIFY(glyph.has_value());

    QVERIFY(sceneModel.selectObjectAt(glyph->x, glyph->y));
    const QVariantMap inspector = sceneModel.selectedObjectInspector();
    QCOMPARE(inspector.value("title").toString(), QString("M31"));
    QCOMPARE(inspectorFieldValue(inspector, "Type"), QString("Galaxy"));
    QVERIFY(inspectorFieldValue(inspector, "Angular size").contains("arcmin"));
    QCOMPARE(inspectorFieldValue(inspector, "Source"), QString("Bundled Messier"));
    QVERIFY(inspector.value("aliases").toString().contains("Andromeda Galaxy"));

    QVERIFY(controller.focusSearchTarget("body", "messier_031"));
    QCOMPARE(sceneModel.selectedObjectInspector().value("title").toString(), QString("M31"));
    QVERIFY(!overlayItemsContainKind(sceneModel.overlayItems(), "selectionLabel"));
}

void SkySceneModelDeepSkyTests::primaryDeepSkyObjectKeepsSourceWhenMergingDeepSkyCatalog()
{
    auto primaryCatalog = createTestCatalog({
        makeDeepSkyBody(
            "primary_dso",
            "Primary DSO",
            8.0,
            {"Primary DSO"}
        ),
        makeDeepSkyBody(
            "messier_031",
            "M31",
            3.44,
            {"M 31", "NGC 224"}
        ),
    });
    QVERIFY(primaryCatalog != nullptr);

    auto deepSkyCatalog = createTestCatalog({
        makeDeepSkyBody(
            "open_ngc_m31",
            "OpenNGC M31",
            3.4,
            {"M 31", "NGC 224", "Andromeda Galaxy"}
        ),
    });
    QVERIFY(deepSkyCatalog != nullptr);

    const auto buildResult = skygate::ui::internal::SkyActiveCatalogBuilder::build(
        skygate::ui::internal::SkyActiveCatalogBuildRequest {
            .sourceCatalog = *primaryCatalog,
            .deepSkyCatalog = deepSkyCatalog.get(),
            .sourceLabel = "Primary",
            .deepSkySourceLabel = "OpenNGC"
        }
    );
    QVERIFY(buildResult.isSuccess());
    QCOMPARE(buildResult.sourceIds.size(), buildResult.catalog->bodies().size());

    const int primarySourceIndex = buildResult.sourceLabels.indexOf("Primary");
    const int deepSkySourceIndex = buildResult.sourceLabels.indexOf("OpenNGC");
    QVERIFY(primarySourceIndex >= 0);
    QVERIFY(deepSkySourceIndex >= 0);

    bool sawPrimaryDso = false;
    bool sawOpenNgcM31 = false;
    for (std::size_t index = 0; index < buildResult.catalog->bodies().size(); ++index) {
        const auto& body = buildResult.catalog->bodies()[index];
        if (body.id == "primary_dso") {
            sawPrimaryDso = true;
            QCOMPARE(buildResult.sourceIds[index], static_cast<std::uint8_t>(primarySourceIndex));
        } else if (body.id == "open_ngc_m31") {
            sawOpenNgcM31 = true;
            QCOMPARE(buildResult.sourceIds[index], static_cast<std::uint8_t>(deepSkySourceIndex));
        } else if (body.id == "messier_031") {
            QFAIL("Merged catalog should replace the primary M31 with the OpenNGC body");
        }
    }

    QVERIFY(sawPrimaryDso);
    QVERIFY(sawOpenNgcM31);
}

void SkySceneModelDeepSkyTests::deepSkyObjectsRenderAndCanBeHidden()
{
    skygate::ephemeris::CelestialBody m31 = makeBody(
        "messier_031",
        "M31",
        skygate::ephemeris::CelestialBodyType::DeepSkyObject,
        3.44,
        skygate::core::EquatorialCoordinate {
            .rightAscensionHours = 0.7123,
            .declinationDeg = 41.269
        }
    );
    m31.deepSkyObject = skygate::ephemeris::DeepSkyObjectInfo {
        .kind = skygate::ephemeris::DeepSkyObjectKind::Galaxy,
        .aliases = {"M31", "Andromeda Galaxy"},
        .majorAxisArcmin = 177.0,
        .minorAxisArcmin = 70.0,
        .positionAngleDeg = 35.0,
    };

    TestSkyContextConfig contextConfig;
    contextConfig.utcDate = QStringLiteral("2024-09-01");
    SkySceneModelTestHarness harness({std::move(m31)}, contextConfig);
    QVERIFY(harness.isValid());
    QVERIFY(harness.centerOnBody("messier_031"));
    SkyContextController& controller = harness.controller();
    const SkySceneModel& sceneModel = harness.sceneModel();

    QVERIFY(!sceneModel.renderGlyphSpan().empty());
    QVERIFY(std::any_of(
        sceneModel.renderGlyphSpan().begin(),
        sceneModel.renderGlyphSpan().end(),
        [&sceneModel](const SkyRenderGlyph& glyph) {
            return sceneModel.objectLabelAt(glyph.x, glyph.y) == "M31";
        }
    ));

    auto* overlayLayers = qobject_cast<SkyOverlayLayerSettings*>(controller.overlayLayers());
    QVERIFY(overlayLayers != nullptr);
    overlayLayers->setDeepSkyObjects(false);

    QVERIFY(sceneModel.renderGlyphSpan().empty());
}

void SkySceneModelDeepSkyTests::denseDeepSkyLabelsAreBudgeted()
{
    std::vector<skygate::ephemeris::CelestialBody> bodies;
    std::vector<skygate::core::HorizontalCoordinate> coordinates;
    bodies.reserve(360U);
    coordinates.reserve(360U);

    for (int index = 0; index < 360; ++index) {
        const int row = index / 24;
        const int column = index % 24;
        bodies.push_back(makeDeepSkyBody(
            "ngc_" + std::to_string(index + 1),
            "NGC " + std::to_string(index + 1),
            9.0 + (static_cast<double>(index % 5) * 0.1)
        ));
        coordinates.push_back(skygate::core::HorizontalCoordinate {
            .altitudeDeg = 45.0 + ((static_cast<double>(row) - 7.0) * 0.16),
            .azimuthDeg = 180.0 + ((static_cast<double>(column) - 12.0) * 0.16)
        });
    }

    const SkyRenderFrame frame = buildDeepSkyRenderFrame(
        std::move(bodies),
        std::move(coordinates),
        10.0
    );

    QCOMPARE(frame.glyphs.size(), 360U);
    QVERIFY(frame.labels.size() > 0);
    QVERIFY(frame.labels.size() <= 100);
}

void SkySceneModelDeepSkyTests::wideDeepSkyLabelsPreferNamedObjects()
{
    std::vector<skygate::ephemeris::CelestialBody> bodies;
    bodies.push_back(makeDeepSkyBody(
        "messier_031",
        "M31",
        3.44,
        {"M 31", "NGC 224", "Andromeda Galaxy"}
    ));
    bodies.push_back(makeDeepSkyBody(
        "ngc_100",
        "NGC 100",
        4.0,
        {"NGC 100"}
    ));
    bodies.push_back(makeDeepSkyBody(
        "ngc_7000",
        "NGC 7000",
        4.0,
        {"NGC 7000", "North America Nebula"}
    ));

    const SkyRenderFrame frame = buildDeepSkyRenderFrame(
        std::move(bodies),
        {
            skygate::core::HorizontalCoordinate {
                .altitudeDeg = 45.0,
                .azimuthDeg = 180.0
            },
            skygate::core::HorizontalCoordinate {
                .altitudeDeg = 45.0,
                .azimuthDeg = 195.0
            },
            skygate::core::HorizontalCoordinate {
                .altitudeDeg = 45.0,
                .azimuthDeg = 165.0
            }
        },
        50.0
    );

    QVERIFY(overlayItemsContainText(frame.labels, "M31"));
    QVERIFY(overlayItemsContainText(frame.labels, "NGC 7000"));
    QVERIFY(!overlayItemsContainText(frame.labels, "NGC 100"));
}

void SkySceneModelDeepSkyTests::deepestDeepSkyZoomShowsSeparatedAnonymousLabels()
{
    std::vector<skygate::ephemeris::CelestialBody> bodies;
    std::vector<skygate::core::HorizontalCoordinate> coordinates;
    bodies.reserve(24U);
    coordinates.reserve(24U);

    for (int index = 0; index < 24; ++index) {
        const int row = index / 6;
        const int column = index % 6;
        bodies.push_back(makeDeepSkyBody(
            "ngc_" + std::to_string(7000 + index),
            "NGC " + std::to_string(7000 + index),
            10.0
        ));
        coordinates.push_back(skygate::core::HorizontalCoordinate {
            .altitudeDeg = 45.0 + ((static_cast<double>(row) - 1.5) * 0.16),
            .azimuthDeg = 180.0 + ((static_cast<double>(column) - 2.5) * 0.16)
        });
    }

    const SkyRenderFrame frame = buildDeepSkyRenderFrame(
        std::move(bodies),
        std::move(coordinates),
        skygate::core::ViewportMath::kFieldOfViewMinDeg
    );

    QCOMPARE(frame.glyphs.size(), 24U);
    QCOMPARE(frame.labels.size(), 24);
    QVERIFY(overlayItemsContainText(frame.labels, "NGC 7000"));
    QVERIFY(overlayItemsContainText(frame.labels, "NGC 7023"));
}

QTEST_GUILESS_MAIN(SkySceneModelDeepSkyTests)

#include "SkySceneModelDeepSkyTests.moc"
