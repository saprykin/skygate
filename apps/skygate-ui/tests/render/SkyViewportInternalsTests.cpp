#include "math/ViewportMath.hpp"
#include "render/SkyCardinalDirectionPolicy.hpp"
#include "render/SkyRenderGlyph.hpp"
#include "render/SkyRenderLine.hpp"
#include "render/SkyRenderPoint.hpp"
#include "render/SkyViewportGeometry.hpp"
#include "render/SkyViewportGeometryInput.hpp"
#include "render/SkyViewportLineSegment.hpp"
#include "render/SkyViewportSceneGraph.hpp"
#include "render/SkyViewportSceneRootNode.hpp"
#include "theme/SkyThemeRenderPalette.hpp"

#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QtTest/QtTest>

#include <algorithm>
#include <optional>
#include <vector>

namespace {

std::optional<skygate::core::PreparedProjection> makeProjection()
{
    return skygate::core::PreparedProjection::create(
        skygate::core::ProjectionType::Stereographic,
        skygate::core::ViewportMath::buildProjectionParams(800.0, 600.0, 45.0, 180.0, 80.0)
    );
}

// Azimuthal views wide enough to reach the horizon in every direction.
std::optional<skygate::core::PreparedProjection> makeWideHorizonProjection(const double centerAzimuthDeg)
{
    return skygate::core::PreparedProjection::create(
        skygate::core::ProjectionType::AzimuthalEquidistant,
        skygate::core::ViewportMath::buildProjectionParams(800.0, 600.0, 0.0, centerAzimuthDeg, 150.0)
    );
}

bool hasSegmentWithColor(
    const std::vector<skygate::ui::internal::SkyViewportLineSegment>& segments, const QColor& color
)
{
    return std::any_of(
        segments.begin(), segments.end(), [&color](const skygate::ui::internal::SkyViewportLineSegment& segment) {
            return segment.color == color;
        }
    );
}

SkyOverlayLayerVisibility hiddenOverlays()
{
    SkyOverlayLayerVisibility overlays;
    overlays.horizon = false;
    overlays.altAzGrid = false;
    overlays.ecliptic = false;
    overlays.celestialEquator = false;
    overlays.circumpolarBoundary = false;
    return overlays;
}

int childCount(const QSGNode& node)
{
    int count = 0;
    for (const QSGNode* child = node.firstChild(); child != nullptr; child = child->nextSibling()) {
        ++count;
    }
    return count;
}

const QSGGeometryNode* firstGeometryChild(const QSGNode& node)
{
    const QSGNode* child = node.firstChild();
    if (child == nullptr || child->type() != QSGNode::GeometryNodeType) {
        return nullptr;
    }
    return static_cast<const QSGGeometryNode*>(child);
}

}  // namespace

class SkyViewportInternalsTests final : public QObject {
    Q_OBJECT

private slots:
    void geometryAppendsFrameLinesAndDeepSkyGlyphSegments();
    void cardinalDirectionPolicyMapsLabelsAndColors();
    void cardinalGridLinesUsePaletteColors();
    void glyphExtentRadiusHonoursMinimum();
    void sceneGraphBucketsAndSkipsDegenerateLines();
    void sceneGraphKeepsDistinctAlphaLineBuckets();
    void sceneGraphPreservesLegacyPackedKeyOrder();
    void sceneGraphBucketsPointsByColor();
    void sceneRootOwnsSeparateLineAndPointRoots();
};

void SkyViewportInternalsTests::geometryAppendsFrameLinesAndDeepSkyGlyphSegments()
{
    const auto projection = makeProjection();
    QVERIFY(projection.has_value());

    skygate::ui::internal::SkyThemeRenderPalette theme;
    theme.gridAltitudeLine = QColor("#111111");
    theme.gridAzimuthLine = QColor("#222222");
    theme.horizonLine = QColor("#333333");
    theme.eclipticLine = QColor("#444444");
    theme.celestialEquatorLine = QColor("#555555");
    theme.circumpolarBoundaryLine = QColor("#666666");

    const std::vector<SkyRenderLine> frameLines{
        {.x1 = 10.0, .y1 = 20.0, .x2 = 30.0, .y2 = 45.0, .widthPx = 2.5, .color = QColor("#abcdef")}
    };
    const std::vector<SkyRenderGlyph> glyphs{
        {.x = 100.0,
         .y = 120.0,
         .radiusXPx = 8.0,
         .radiusYPx = 6.0,
         .rotationDeg = 0.0,
         .widthPx = 1.5,
         .kind = skygate::ephemeris::DeepSkyObjectInfo::Kind::GlobularCluster,
         .color = QColor("#fedcba")}
    };

    const auto segments = skygate::ui::internal::SkyViewportGeometry::buildLineSegments(
        skygate::ui::internal::SkyViewportGeometryInput{
            .projection = *projection,
            .viewportWidth = 800.0,
            .viewportHeight = 600.0,
            .renderTheme = theme,
            .overlayLayers = hiddenOverlays(),
            .observer = {},
            .utcTime = {},
            .renderLines = frameLines,
            .renderGlyphs = glyphs
        }
    );

    QCOMPARE(segments.size(), 35U);
    QCOMPARE(segments.front().x1, 10.0F);
    QCOMPARE(segments.front().y1, 20.0F);
    QCOMPARE(segments.front().x2, 30.0F);
    QCOMPARE(segments.front().y2, 45.0F);
    QCOMPARE(segments.front().widthPx, 2.5F);
    QCOMPARE(segments.front().color, QColor("#abcdef"));

    const auto glyphSegmentCount = std::count_if(
        segments.begin(), segments.end(), [](const skygate::ui::internal::SkyViewportLineSegment& segment) {
            return segment.color == QColor("#fedcba");
        }
    );
    QCOMPARE(glyphSegmentCount, 34);
}

void SkyViewportInternalsTests::cardinalDirectionPolicyMapsLabelsAndColors()
{
    skygate::ui::internal::SkyThemeRenderPalette theme;
    theme.cardinalNorthLine = QColor("#010101");
    theme.cardinalEastLine = QColor("#020202");
    theme.cardinalSouthLine = QColor("#030303");
    theme.cardinalWestLine = QColor("#040404");
    theme.gridAzimuthLine = QColor("#050505");

    const auto& azimuths = skygate::ui::internal::SkyCardinalDirectionPolicy::kAzimuthsDeg;
    QCOMPARE(azimuths.size(), 4U);
    QCOMPARE(azimuths[0], 0.0);
    QCOMPARE(azimuths[1], 90.0);
    QCOMPARE(azimuths[2], 180.0);
    QCOMPARE(azimuths[3], 270.0);

    QCOMPARE(skygate::ui::internal::SkyCardinalDirectionPolicy::label(0.0), QString("N"));
    QCOMPARE(skygate::ui::internal::SkyCardinalDirectionPolicy::label(90.0), QString("E"));
    QCOMPARE(skygate::ui::internal::SkyCardinalDirectionPolicy::label(180.0), QString("S"));
    QCOMPARE(skygate::ui::internal::SkyCardinalDirectionPolicy::label(270.0), QString("W"));
    QVERIFY(skygate::ui::internal::SkyCardinalDirectionPolicy::label(45.0).isEmpty());

    QCOMPARE(skygate::ui::internal::SkyCardinalDirectionPolicy::lineColor(0.0, theme), theme.cardinalNorthLine);
    QCOMPARE(skygate::ui::internal::SkyCardinalDirectionPolicy::lineColor(90.0, theme), theme.cardinalEastLine);
    QCOMPARE(skygate::ui::internal::SkyCardinalDirectionPolicy::lineColor(180.0, theme), theme.cardinalSouthLine);
    QCOMPARE(skygate::ui::internal::SkyCardinalDirectionPolicy::lineColor(270.0, theme), theme.cardinalWestLine);
    QCOMPARE(skygate::ui::internal::SkyCardinalDirectionPolicy::lineColor(45.0, theme), theme.gridAzimuthLine);
}

void SkyViewportInternalsTests::cardinalGridLinesUsePaletteColors()
{
    skygate::ui::internal::SkyThemeRenderPalette theme;
    theme.gridAltitudeLine = QColor("#101010");
    theme.gridAzimuthLine = QColor("#202020");
    theme.cardinalNorthLine = QColor("#303030");
    theme.cardinalEastLine = QColor("#404040");
    theme.cardinalSouthLine = QColor("#505050");
    theme.cardinalWestLine = QColor("#606060");
    theme.horizonLine = QColor("#707070");

    SkyOverlayLayerVisibility overlays;
    overlays.horizon = false;

    const auto northEastProjection = makeWideHorizonProjection(45.0);
    QVERIFY(northEastProjection.has_value());
    const auto northEastSegments = skygate::ui::internal::SkyViewportGeometry::buildLineSegments(
        skygate::ui::internal::SkyViewportGeometryInput{
            .projection = *northEastProjection,
            .viewportWidth = 800.0,
            .viewportHeight = 600.0,
            .renderTheme = theme,
            .overlayLayers = overlays
        }
    );
    QVERIFY(hasSegmentWithColor(northEastSegments, theme.cardinalNorthLine));
    QVERIFY(hasSegmentWithColor(northEastSegments, theme.cardinalEastLine));

    const auto southWestProjection = makeWideHorizonProjection(225.0);
    QVERIFY(southWestProjection.has_value());
    const auto southWestSegments = skygate::ui::internal::SkyViewportGeometry::buildLineSegments(
        skygate::ui::internal::SkyViewportGeometryInput{
            .projection = *southWestProjection,
            .viewportWidth = 800.0,
            .viewportHeight = 600.0,
            .renderTheme = theme,
            .overlayLayers = overlays
        }
    );
    QVERIFY(hasSegmentWithColor(southWestSegments, theme.cardinalSouthLine));
    QVERIFY(hasSegmentWithColor(southWestSegments, theme.cardinalWestLine));
}

void SkyViewportInternalsTests::glyphExtentRadiusHonoursMinimum()
{
    SkyRenderGlyph glyph{.x = 10.0, .y = 20.0, .radiusXPx = 3.0, .radiusYPx = 5.0};
    QCOMPARE(glyph.extentRadiusPx(8.0), 8.0);
    QCOMPARE(glyph.extentRadiusPx(12.0), 12.0);

    glyph.radiusYPx = 18.0;
    QCOMPARE(glyph.extentRadiusPx(8.0), 18.0);
    QCOMPARE(glyph.extentRadiusPx(12.0), 18.0);

    glyph.radiusXPx = 30.0;
    QCOMPARE(glyph.extentRadiusPx(8.0), 30.0);
    QCOMPARE(glyph.extentRadiusPx(12.0), 30.0);
}

void SkyViewportInternalsTests::sceneGraphBucketsAndSkipsDegenerateLines()
{
    QSGNode root;
    const std::vector<skygate::ui::internal::SkyViewportLineSegment> segments{
        {.x1 = 0.0F, .y1 = 0.0F, .x2 = 10.0F, .y2 = 0.0F, .widthPx = 2.0F, .color = QColor("#ff0000")},
        {.x1 = 4.0F, .y1 = 4.0F, .x2 = 4.0F, .y2 = 12.0F, .widthPx = 2.0F, .color = QColor("#ff0000")},
        {.x1 = 5.0F, .y1 = 5.0F, .x2 = 5.0F, .y2 = 5.0F, .widthPx = 2.0F, .color = QColor("#ff0000")},
    };

    skygate::ui::internal::SkyViewportSceneGraph::syncLineNodes(&root, segments);

    QCOMPARE(childCount(root), 1);
    const QSGGeometryNode* geometryNode = firstGeometryChild(root);
    QVERIFY(geometryNode != nullptr);
    QCOMPARE(geometryNode->geometry()->vertexCount(), 12);
    QCOMPARE(geometryNode->geometry()->drawingMode(), static_cast<unsigned int>(QSGGeometry::DrawTriangles));
    QCOMPARE(static_cast<QSGFlatColorMaterial*>(geometryNode->material())->color(), QColor("#ff0000"));

    skygate::ui::internal::SkyViewportSceneGraph::clearChildNodes(&root);
    QCOMPARE(childCount(root), 0);
}

void SkyViewportInternalsTests::sceneGraphKeepsDistinctAlphaLineBuckets()
{
    QColor lowAlphaColor = QColor("#3366ff");
    lowAlphaColor.setAlpha(50);
    QColor highAlphaColor = QColor("#3366ff");
    highAlphaColor.setAlpha(200);

    QSGNode root;
    const std::vector<skygate::ui::internal::SkyViewportLineSegment> segments{
        {.x1 = 0.0F, .y1 = 10.0F, .x2 = 20.0F, .y2 = 10.0F, .widthPx = 1.5F, .color = lowAlphaColor},
        {.x1 = 0.0F, .y1 = 30.0F, .x2 = 20.0F, .y2 = 30.0F, .widthPx = 1.5F, .color = highAlphaColor},
    };

    skygate::ui::internal::SkyViewportSceneGraph::syncLineNodes(&root, segments);

    QCOMPARE(childCount(root), 2);
    std::vector<QColor> nodeColors;
    int totalVertices = 0;
    for (const QSGNode* child = root.firstChild(); child != nullptr; child = child->nextSibling()) {
        QVERIFY(child->type() == QSGNode::GeometryNodeType);
        const auto* geometryNode = static_cast<const QSGGeometryNode*>(child);
        QCOMPARE(geometryNode->geometry()->vertexCount(), 6);
        totalVertices += geometryNode->geometry()->vertexCount();
        nodeColors.push_back(static_cast<const QSGFlatColorMaterial*>(geometryNode->material())->color());
    }

    QCOMPARE(totalVertices, 12);
    QCOMPARE(nodeColors.size(), 2U);
    QCOMPARE(nodeColors.front(), lowAlphaColor);
    QCOMPARE(nodeColors.back(), highAlphaColor);
}

void SkyViewportInternalsTests::sceneGraphPreservesLegacyPackedKeyOrder()
{
    QColor translucentRed = QColor("#ff0000");
    translucentRed.setAlpha(50);
    const QColor opaqueBlue = QColor("#0000ff");

    QSGNode root;
    const std::vector<skygate::ui::internal::SkyViewportLineSegment> segments{
        {.x1 = 0.0F, .y1 = 10.0F, .x2 = 20.0F, .y2 = 10.0F, .widthPx = 1.5F, .color = translucentRed},
        {.x1 = 0.0F, .y1 = 30.0F, .x2 = 20.0F, .y2 = 30.0F, .widthPx = 1.5F, .color = opaqueBlue},
    };

    skygate::ui::internal::SkyViewportSceneGraph::syncLineNodes(&root, segments);

    QCOMPARE(childCount(root), 2);
    const QSGGeometryNode* firstNode = firstGeometryChild(root);
    QVERIFY(firstNode != nullptr);
    const auto* secondNode = static_cast<const QSGGeometryNode*>(firstNode->nextSibling());
    QCOMPARE(static_cast<const QSGFlatColorMaterial*>(firstNode->material())->color(), opaqueBlue);
    QCOMPARE(static_cast<const QSGFlatColorMaterial*>(secondNode->material())->color(), translucentRed);
}

void SkyViewportInternalsTests::sceneGraphBucketsPointsByColor()
{
    QSGNode root;
    const std::vector<SkyRenderPoint> points{
        {.x = 10.0, .y = 20.0, .sizePx = 4.0, .color = QColor("#00ff00")},
        {.x = 30.0, .y = 40.0, .sizePx = 6.0, .color = QColor("#00ff00")},
        {.x = 50.0, .y = 60.0, .sizePx = 8.0, .color = QColor("#0000ff")},
    };

    skygate::ui::internal::SkyViewportSceneGraph::syncPointNodes(&root, points);

    QCOMPARE(childCount(root), 2);
    int totalVertices = 0;
    for (const QSGNode* child = root.firstChild(); child != nullptr; child = child->nextSibling()) {
        QVERIFY(child->type() == QSGNode::GeometryNodeType);
        totalVertices += static_cast<const QSGGeometryNode*>(child)->geometry()->vertexCount();
    }
    QCOMPARE(totalVertices, 18);
}

void SkyViewportInternalsTests::sceneRootOwnsSeparateLineAndPointRoots()
{
    skygate::ui::internal::SkyViewportSceneRootNode root;

    QVERIFY(root.lineRoot() != nullptr);
    QVERIFY(root.pointRoot() != nullptr);
    QVERIFY(root.lineRoot() != root.pointRoot());
    QCOMPARE(root.firstChild(), root.lineRoot());
    QCOMPARE(root.lineRoot()->nextSibling(), root.pointRoot());
}

QTEST_APPLESS_MAIN(SkyViewportInternalsTests)

#include "SkyViewportInternalsTests.moc"
